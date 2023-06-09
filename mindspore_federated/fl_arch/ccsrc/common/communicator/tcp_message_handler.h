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

#ifndef MINDSPORE_CCSRC_FL_COMMUNICATOR_TCP_MESSAGE_HANDLER_H_
#define MINDSPORE_CCSRC_FL_COMMUNICATOR_TCP_MESSAGE_HANDLER_H_

#include <functional>
#include <iostream>
#include <string>
#include <memory>
#include <vector>

#include "common/utils/log_adapter.h"
#include "common/communicator/message.h"
#include "common/protos/comm.pb.h"
#include "common/utils/convert_utils_base.h"
#include "common/constants.h"

namespace mindspore {
namespace fl {
constexpr size_t kHeaderLen = sizeof(MessageHeader);

class TcpMessageHandler {
 public:
  using MessageHandleFun = std::function<void(const MessageMeta &, const Protos &, const VectorPtr &)>;
  void SetCallback(const MessageHandleFun &cb) { msg_callback_ = cb; }

  using ReadBufferFun = std::function<size_t(void *, size_t max_size)>;
  void ReceiveMessage(const ReadBufferFun &read_fun);

 private:
  size_t cur_header_len_ = 0;
  size_t cur_meta_len_ = 0;
  size_t cur_data_len_ = 0;

  uint8_t header_[kHeaderLen]{0};
  std::vector<uint8_t> meta_buffer_;
  VectorPtr data_;
  MessageHeader message_header_;
  MessageMeta message_meta_;
  MessageHandleFun msg_callback_ = nullptr;

  bool ReceiveMessageInner(const ReadBufferFun &read_fun, bool *end_read);
  bool ReadMessageHeader(const ReadBufferFun &read_fun, bool *end_read);
  bool ReadMessageMeta(const ReadBufferFun &read_fun, bool *end_read);
  bool ReadMessageDataAndCallback(const ReadBufferFun &read_fun, bool *end_read);
  void Reset();
};
}  // namespace fl
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_FL_COMMUNICATOR_TCP_MESSAGE_HANDLER_H_
