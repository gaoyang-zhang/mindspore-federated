/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "server/iteration_metrics.h"

#include <fstream>
#include <string>

#include "common/constants.h"
#include "common/core/comm_util.h"

namespace mindspore {
namespace fl {
namespace server {
bool IterationMetrics::Initialize() {
  metrics_file_path_ = FLContext::instance()->metrics_file();
  FileConfig metrics_config;
  if (metrics_file_path_.empty()) {
    MS_LOG(WARNING) << "Metrics parament in config is not correct";
    return false;
  }
  if (CommUtil::CreateDirectory(metrics_file_path_)) {
    MS_LOG(INFO) << "Create Directory :" << metrics_file_path_ << " success.";
  }
  metrics_file_.open(metrics_file_path_.c_str(), std::ios::app | std::ios::out);
  metrics_file_.close();
  return true;
}

bool IterationMetrics::Summarize() {
  metrics_file_.open(metrics_file_path_, std::ios::out | std::ios::app);
  if (!metrics_file_.is_open()) {
    MS_LOG(ERROR) << "The metrics file is not opened.";
    return false;
  }

  js_[kInstanceName] = instance_name_;
  js_[kStartTime] = start_time_.time_str_mill;
  js_[kEndTime] = end_time_.time_str_mill;
  js_[kFLName] = fl_name_;
  js_[kInstanceStatus] = kInstanceStateName.at(instance_state_);
  js_[kFLIterationNum] = fl_iteration_num_;
  js_[kCurIteration] = cur_iteration_num_;
  js_[kMetricsAuc] = accuracy_;
  js_[kMetricsLoss] = loss_;
  js_[kIterExecutionTime] = iteration_time_cost_;
  js_[kClientVisitedInfo] = round_client_num_map_;
  js_[kIterationResult] = iteration_result_ ? "success" : "fail";
  js_[kMetricsUnsupervisedEval] = unsupervised_eval_;

  metrics_file_ << js_ << "\n";
  (void)metrics_file_.flush();
  metrics_file_.close();
  return true;
}

bool IterationMetrics::Clear() {
  if (metrics_file_.is_open()) {
    MS_LOG(INFO) << "Clear the old metrics file " << metrics_file_path_;
    metrics_file_.close();
    metrics_file_.open(metrics_file_path_, std::ios::ate | std::ios::out);
  }
  return true;
}

void IterationMetrics::set_fl_name(const std::string &fl_name) { fl_name_ = fl_name; }

void IterationMetrics::set_fl_iteration_num(size_t fl_iteration_num) { fl_iteration_num_ = fl_iteration_num; }

void IterationMetrics::set_cur_iteration_num(size_t cur_iteration_num) { cur_iteration_num_ = cur_iteration_num; }

void IterationMetrics::set_instance_state(cache::InstanceState state) { instance_state_ = state; }

void IterationMetrics::set_loss(float loss) { loss_ = loss; }

void IterationMetrics::set_accuracy(float acc) { accuracy_ = acc; }

void IterationMetrics::set_iteration_time_cost(uint64_t iteration_time_cost) {
  iteration_time_cost_ = iteration_time_cost;
}

void IterationMetrics::set_round_client_num_map(const std::map<std::string, size_t> round_client_num_map) {
  round_client_num_map_ = round_client_num_map;
}

void IterationMetrics::set_iteration_result(bool iteration_result) { iteration_result_ = iteration_result; }

void IterationMetrics::SetStartTime(const Time &start_time) { start_time_ = start_time; }

void IterationMetrics::SetEndTime(const Time &end_time) { end_time_ = end_time; }

void IterationMetrics::SetInstanceName(const std::string &instance_name) { instance_name_ = instance_name; }

void IterationMetrics::set_unsupervised_eval(const float &unsupervised_eval) { unsupervised_eval_ = unsupervised_eval; }
}  // namespace server
}  // namespace fl
}  // namespace mindspore
