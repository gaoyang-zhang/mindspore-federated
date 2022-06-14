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

#ifndef MINDSPORE_CCSRC_FL_SERVER_KERNEL_PUSH_WEIGHT_KERNEL_H_
#define MINDSPORE_CCSRC_FL_SERVER_KERNEL_PUSH_WEIGHT_KERNEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "common/common.h"
#include "server/kernel/round/round_kernel.h"
#include "server/kernel/round/round_kernel_factory.h"
#include "server/executor.h"

namespace mindspore {
namespace fl {
namespace server {
namespace kernel {
class PushWeightKernel : public RoundKernel {
 public:
  PushWeightKernel() = default;

  void InitKernel(size_t threshold_count) override;
  bool Launch(const uint8_t *req_data, size_t len, const std::shared_ptr<MessageHandler> &message) override;

 private:
  void BuildPushWeightRsp(FBBuilder *fbb, const schema::ResponseCode retcode, const std::string &reason,
                          size_t iteration);
  FlStatus OnReceiveModelWeight(const uint8_t *req_data, size_t len);
  std::map<std::string, Address> ParseFeatureMap(const schema::RequestPushWeight *push_weight_req);
};
}  // namespace kernel
}  // namespace server
}  // namespace fl
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_FL_SERVER_KERNEL_PUSH_WEIGHT_KERNEL_H_
