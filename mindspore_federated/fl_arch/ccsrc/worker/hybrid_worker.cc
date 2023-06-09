/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include "common/exit_handler.h"
#include "worker/hybrid_worker.h"
#include "armour/secure_protocol/key_agreement.h"
#include "distributed_cache/distributed_cache.h"
#include "distributed_cache/worker.h"
#include "distributed_cache/scheduler.h"

namespace mindspore {
namespace fl {
namespace worker {
HybridWorker &HybridWorker::GetInstance() {
  static HybridWorker instance;
  return instance;
}

void HybridWorker::Init(const std::map<std::string, std::vector<float>> &initial_model) {
  if (running_.load()) {
    MS_LOG_EXCEPTION << "Worker has been inited";
  }
  running_ = true;
  ExitHandler::Instance().InitSignalHandle();

  InitAndLoadDistributedCache();
  worker_node_ = std::make_shared<fl::WorkerNode>();
  MS_EXCEPTION_IF_NULL(worker_node_);
  fl_name_ = FLContext::instance()->fl_name();
  MS_LOG(INFO) << "Fl name is " << fl_name_;
  initial_model_ = initial_model;
  if (!worker_node_->Start()) {
    MS_LOG(EXCEPTION) << "Starting worker node failed.";
  }
  StartPeriodJob();
}

void HybridWorker::StartPeriodJob() {
  cache::Worker::Instance().Init(fl_id(), FLContext::instance()->fl_name());
  constexpr int retry_times = 15 * 60;
  constexpr int kWorkerRegisterInterval = 1;
  cache::CacheStatus status = cache::CacheStatusCode::kCacheNil;
  MS_LOG(INFO) << "Hybrid worker try to register to distributed cache.";
  for (int i = 0; i < retry_times; i++) {
    status = cache::Worker::Instance().Register();
    if (!status.IsSuccess()) {
      std::this_thread::sleep_for(std::chrono::seconds(kWorkerRegisterInterval));
    } else {
      break;
    }
  }
  if (!status.IsSuccess()) {
    MS_LOG(EXCEPTION) << "Register worker to distributed cache failed.";
  }

  auto client = cache::DistributedCacheLoader::Instance().GetOneClient();
  if (client == nullptr) {
    MS_LOG(EXCEPTION) << "Get redis client failed";
  }
  auto ret = cache::Scheduler::Instance().GetInstanceName(fl_name_, &instance_name_);
  if (!ret.IsSuccess() || instance_name_.empty()) {
    MS_LOG(EXCEPTION) << "Get instance name failed.";
  }
  MS_LOG_INFO << "Register worker to distributed cache successfully, instance name is " << instance_name_;
  auto period_fun = [&]() {
    auto cache_link_valid = true;
    while (!ExitHandler::Instance().HasStopped()) {
      constexpr int default_sync_duration_ms = 1000;  // 1000ms
      std::this_thread::sleep_for(std::chrono::milliseconds(default_sync_duration_ms));

      auto cache_ret = cache::Worker::Instance().Sync();
      if (!cache_ret.IsSuccess()) {
        constexpr int64_t log_interval = 15000;  // 15s
        static int64_t last_retry_timestamp_ms = 0;
        if (cache_link_valid) {
          cache_link_valid = false;
          last_retry_timestamp_ms = 0;
        }
        int64_t current_timestamp_ms = CURRENT_TIME_MILLI.count();
        if (current_timestamp_ms - last_retry_timestamp_ms >= log_interval) {
          last_retry_timestamp_ms = current_timestamp_ms;
          MS_LOG_ERROR << "Failed to reconnect to distributed cache: " << cache_ret.GetDetail();
        }
      } else {
        if (!cache_link_valid) {
          cache_link_valid = true;
          MS_LOG_INFO << "Success to reconnect to distributed cache";
        }
        std::string instance_name;
        auto ret = cache::Scheduler::Instance().GetInstanceName(fl_name_, &instance_name);
        if (ret.IsSuccess() && !instance_name.empty() && instance_name != instance_name_) {
          instance_name_ = instance_name;
          MS_LOG_INFO << "Server cluster is processing new instance, new instance name is " << instance_name_;
        }
      }
    }
  };
  period_thread_ = std::thread(period_fun);
}

void HybridWorker::Stop() {
  MS_LOG_INFO << "Start to stop worker";
  ExitHandler::Instance().SetStopFlag();
  if (period_thread_.joinable()) {
    period_thread_.join();
  }
  cache::Worker::Instance().Stop();
  MS_LOG_INFO << "Stop worker successfully";
}

void HybridWorker::InitAndLoadDistributedCache() {
  auto config = FLContext::instance()->distributed_cache_config();
  if (config.address.empty()) {
    MS_LOG(EXCEPTION) << "Distributed cache address cannot be empty.";
  }
  if (!cache::DistributedCacheLoader::Instance().InitCacheImpl(config)) {
    MS_LOG(EXCEPTION) << "Link to distributed cache failed, distributed cache address: " << config.address
                      << ", enable ssl: " << FLContext::instance()->enable_ssl();
  }
}

bool HybridWorker::SendToServer(const void *data, size_t size, TcpUserCommand command, VectorPtr *output) {
  MS_EXCEPTION_IF_NULL(worker_node_);
  MS_EXCEPTION_IF_NULL(data);
  if (output != nullptr) {
    while (!ExitHandler::Instance().HasStopped()) {
      if (!worker_node_->Send(NodeRole::SERVER, data, size, static_cast<int>(command), output, kWorkerTimeout)) {
        MS_LOG(ERROR) << "Sending message to server failed.";
        return false;
      }
      if (*output == nullptr) {
        MS_LOG(WARNING) << "Response from server is empty.";
        return false;
      }

      std::string response_str = std::string(reinterpret_cast<char *>((*output)->data()), (*output)->size());
      if (response_str == kClusterSafeMode || response_str == kJobNotAvailable) {
        MS_LOG(INFO) << "The server is in safe mode, or is disabled or finished.";
        std::this_thread::sleep_for(std::chrono::milliseconds(kWorkerRetryDurationForSafeMode));
      } else {
        break;
      }
    }
  } else {
    if (!worker_node_->Send(NodeRole::SERVER, data, size, static_cast<int>(command), nullptr, kWorkerTimeout)) {
      MS_LOG(ERROR) << "Sending message to server failed.";
      return false;
    }
  }
  return true;
}

void HybridWorker::SetIterationRunning() {
  MS_LOG(INFO) << "Worker iteration starts.";
  worker_iteration_state_ = IterationState::kRunning;
}

void HybridWorker::SetIterationCompleted() {
  MS_LOG(INFO) << "Worker iteration completes.";
  worker_iteration_state_ = IterationState::kCompleted;
}

void HybridWorker::set_fl_iteration_num(uint64_t iteration_num) { iteration_num_ = iteration_num; }

uint64_t HybridWorker::fl_iteration_num() const { return iteration_num_.load(); }

void HybridWorker::set_data_size(int data_size) { data_size_ = data_size; }

int HybridWorker::data_size() const { return data_size_; }

std::string HybridWorker::fl_name() const { return fl_name_; }

std::string HybridWorker::fl_id() const {
  MS_EXCEPTION_IF_NULL(worker_node_);
  return worker_node_->fl_id();
}

std::string HybridWorker::instance_name() const { return instance_name_; }

const std::map<std::string, std::vector<float>> &HybridWorker::initial_model() const { return initial_model_; }
}  // namespace worker
}  // namespace fl
}  // namespace mindspore
