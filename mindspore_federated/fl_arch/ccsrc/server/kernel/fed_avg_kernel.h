/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_FL_SERVER_KERNEL_FED_AVG_KERNEL_H_
#define MINDSPORE_CCSRC_FL_SERVER_KERNEL_FED_AVG_KERNEL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <map>
#include <cmath>
#include "common/common.h"
#include "server/collective_ops_impl.h"
#include "server/local_meta_store.h"
#include "server/executor.h"

namespace mindspore {
namespace fl {
namespace server {
namespace kernel {
// The implementation for the federated average. We do weighted average for the weights. The uploaded weights from
// FL-clients is already multiplied by its data size so only sum and division are done in this kernel.

// Pay attention that this kernel is the distributed version of federated average, which means each server node in the
// cluster in invalid in the aggregation process. So the DistributedCountService and CollectiveOpsImpl are called.
template <typename T, typename S>
class FedAvgKernel {
 public:
  static bool AllReduce(const std::map<std::string, std::string> &server_map, ParamAggregationInfo *info) {
    if (info == nullptr) {
      return false;
    }
    T *weight_addr = reinterpret_cast<T *>(info->weight_data);
    if (!CollectiveOpsImpl::GetInstance().AllReduce<T>(info->name, weight_addr, weight_addr,
                                                       info->weight_size / sizeof(T), server_map)) {
      MS_LOG(ERROR) << "Federated average allreduce failed.";
      return false;
    }
    if (!CollectiveOpsImpl::GetInstance().AllReduce<S>(info->name + "_data_size", &info->data_size, &info->data_size, 1,
                                                       server_map)) {
      MS_LOG(ERROR) << "Federated average allreduce failed.";
      return false;
    }
    auto data_size = info->data_size;
    if (data_size == 0) {
      *info->require_aggr = false;
      MS_LOG(INFO) << "Parameter:" << info->name << " data size is 0, do not need to run fed avg.";
      return true;
    }
    LocalMetaStore::GetInstance().put_value(kCtxFedAvgTotalDataSize, data_size);
    auto elem_num = info->weight_size / sizeof(T);
    for (size_t i = 0; i < elem_num; i++) {
      weight_addr[i] /= data_size;
    }
    return true;
  }

  static bool ScaffoldAllReduce(const std::map<std::string, std::string> &server_map, ParamAggregationInfo *info) {
    MS_EXCEPTION_IF_NULL(info);
    T *weight_addr = reinterpret_cast<T *>(info->weight_data);
    if (!CollectiveOpsImpl::GetInstance().AllReduce<T>(info->name, weight_addr, weight_addr,
                                                       info->weight_size / sizeof(T), server_map)) {
      MS_LOG(ERROR) << "Federated average allreduce failed.";
      return false;
    }
    auto elem_num = info->weight_size / sizeof(T);
    size_t total_client_num = FLContext::instance()->total_client_num();
    if (total_client_num == 0) {
      MS_LOG(ERROR) << "total_client_num is 0.";
      return false;
    }
    for (size_t i = 0; i < elem_num; i++) {
      weight_addr[i] /= total_client_num;
    }
    return true;
  }

  static bool FedNovaAllReduce(const std::map<std::string, std::string> &server_map, ParamAggregationInfo *info) {
    uint64_t start_fl_job_threshold = FLContext::instance()->start_fl_job_threshold();
    float update_model_ratio = FLContext::instance()->update_model_ratio();
    if (start_fl_job_threshold == 0 || update_model_ratio == 0) {
      MS_LOG(ERROR) << "FedNovaAllReduce failed: start_fl_job_threshold or update_model_ratio in yaml file is "
                       "incorrectly set to zero.";
      return false;
    }
    float fednova_weight = pow(start_fl_job_threshold / update_model_ratio, 2);
    MS_EXCEPTION_IF_NULL(info);
    T *weight_addr = reinterpret_cast<T *>(info->weight_data);
    if (!CollectiveOpsImpl::GetInstance().AllReduce<T>(info->name, weight_addr, weight_addr,
                                                       info->weight_size / sizeof(T), server_map)) {
      MS_LOG(ERROR) << "FedNovaAllReduce allreduce weight failed.";
      return false;
    }
    if (!CollectiveOpsImpl::GetInstance().AllReduce<S>(info->name + "_data_size", &info->data_size, &info->data_size, 1,
                                                       server_map)) {
      MS_LOG(ERROR) << "FedNovaAllReduce allreduce data_size failed.";
      return false;
    }
    auto train_step_num = info->data_size;
    if (train_step_num == 0) {
      *info->require_aggr = false;
      MS_LOG(INFO) << "Parameter:" << info->name << " train steps is 0, do not need to run FedNova.";
      return true;
    }
    auto elem_num = info->weight_size / sizeof(T);
    for (size_t i = 0; i < elem_num; i++) {
      weight_addr[i] = train_step_num * weight_addr[i] / fednova_weight;
    }
    return true;
  }

  static void Launch(const Address &update_weight, size_t update_data_size, ParamAggregationInfo *info) {
    if (info == nullptr) {
      return;
    }
    // The weight and new_weight values should be multiplied by clients already, so we don't need to do multiplication
    // again.
    auto weight_addr = reinterpret_cast<T *>(info->weight_data);
    auto new_weight_addr = reinterpret_cast<const T *>(update_weight.addr);

    auto elem_num = info->weight_size / sizeof(T);
    for (size_t i = 0; i < elem_num; i++) {
      weight_addr[i] += new_weight_addr[i];
    }
    info->data_size += update_data_size;
  }
};
}  // namespace kernel
}  // namespace server
}  // namespace fl
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_FL_SERVER_KERNEL_FED_AVG_KERNEL_H_
