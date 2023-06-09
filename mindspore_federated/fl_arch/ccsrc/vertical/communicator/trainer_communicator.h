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

#ifndef MINDSPORE_FL_ARCH_CCSRC_VERTICAL_TRAINER_COMMUNICATOR_H_
#define MINDSPORE_FL_ARCH_CCSRC_VERTICAL_TRAINER_COMMUNICATOR_H_

#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "vertical/communicator/abstract_communicator.h"
#include "vertical/common.h"
#include "common/communicator/http_client.h"
#include "common/protos/vfl.pb.h"
#include "vertical/python/tensor_list_py.h"
#include "vertical/python/tensor_py.h"
#include "vertical/communicator/message_queue.h"

namespace mindspore {
namespace fl {
class TrainerCommunicator : public AbstractCommunicator {
 public:
  TrainerCommunicator() = default;
  ~TrainerCommunicator() = default;

  bool LaunchMsgHandler(const std::shared_ptr<MessageHandler> &message) override;

  void InitCommunicator(const std::shared_ptr<HttpCommunicator> &http_communicator) override;

  bool Send(const std::string &target_server_name, const TensorListItemPy &tensorListItemPy);

  TensorListItemPy Receive(const std::string &target_server_name, const uint32_t &timeout = 100000);

 private:
  bool VerifyTensorListItem(const TensorListItemPy &tensorListItemPy);

  std::mutex message_received_mutex_;

  std::map<std::string, std::shared_ptr<MessageQueue<TensorListItemPy>>> message_queues_ = {};
};
}  // namespace fl
}  // namespace mindspore
#endif  // MINDSPORE_FL_ARCH_CCSRC_VERTICAL_TRAINER_COMMUNICATOR_H_
