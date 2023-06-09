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

#include "vertical/communicator/abstract_communicator.h"
#include "common/communicator/communicator_base.h"
#include "common/communicator/message_handler.h"

namespace mindspore {
namespace fl {
std::shared_ptr<HttpCommunicator> AbstractCommunicator::CreateHttpCommunicator() {
  const auto &vertical_fl_context = VFLContext::instance();
  const auto &fl_context = FLContext::instance();
  if (vertical_fl_context->enable_ssl()) {
    fl_context->set_enable_ssl(vertical_fl_context->enable_ssl());
    fl_context->set_client_password(vertical_fl_context->client_password());
    fl_context->set_server_password(vertical_fl_context->server_password());
    fl_context->set_ssl_config(vertical_fl_context->ssl_config());
  }
  auto http_server_address = VFLContext::instance()->http_server_address();
  std::string server_ip;
  uint32_t http_port = 0;
  if (!CommUtil::SplitIpAddress(http_server_address, &server_ip, &http_port)) {
    MS_LOG_EXCEPTION << "The format of http server address '" << http_server_address << "' is invalid";
  }
  MS_LOG(INFO) << "Create Http communicator.";
  auto http_server = std::make_shared<HttpServer>(server_ip, http_port, kThreadNum);
  MS_EXCEPTION_IF_NULL(http_server);
  auto http_communicator = std::make_shared<HttpCommunicator>(http_server);
  MS_EXCEPTION_IF_NULL(http_communicator);
  return http_communicator;
}

void AbstractCommunicator::StartHttpServer(const std::shared_ptr<HttpCommunicator> &http_communicator) {
  if (http_communicator == nullptr) {
    MS_LOG(EXCEPTION) << "Http server starting failed.";
  }
  http_communicator->Start();
  auto http_server = http_communicator->http_server();
  MS_EXCEPTION_IF_NULL(http_server);
  MS_LOG(INFO) << "Initialize http server IP:" << http_server->address() << ", PORT:" << http_server->port();
  if (!http_server->Start()) {
    MS_LOG(EXCEPTION) << "Http server starting failed.";
  }
  MS_LOG(INFO) << "Http communicator starte successfully.";
}

void AbstractCommunicator::RegisterMsgCallBack(const std::shared_ptr<HttpCommunicator> &http_communicator,
                                               const std::string &name) {
  MS_EXCEPTION_IF_NULL(http_communicator);
  MS_LOG(INFO) << "Vertical communicator register message callback for " << name;
  http_communicator->RegisterRoundMsgCallback(
    name, [this](const std::shared_ptr<MessageHandler> &message) { LaunchMsgHandler(message); });
}

void AbstractCommunicator::InitHttpClient() {
  remote_server_address_ = VFLContext::instance()->remote_server_address();
  if (VFLContext::instance()->enable_ssl()) {
    for (const auto &item : remote_server_address_) {
      auto target_server_name = item.first;
      auto server_address = item.second;
      remote_server_address_[target_server_name] = "https://" + server_address;
    }
  } else {
    for (const auto &item : remote_server_address_) {
      auto target_server_name = item.first;
      auto server_address = item.second;
      remote_server_address_[target_server_name] = "http://" + server_address;
    }
  }
  MS_LOG(INFO) << "Request will be sent to server domain:" << remote_server_address_;
  for (const auto &item : remote_server_address_) {
    auto target_server_name = item.first;
    auto server_address = item.second;
    auto http_client = std::make_shared<HttpClient>(server_address);

    http_client->SetMessageCallback([&](const std::shared_ptr<ResponseTrack> &response_track,
                                        const std::string &msg_type) { NotifyMessageArrival(response_track); });
    http_client->Init();
    http_clients_[target_server_name] = http_client;
  }
}

std::shared_ptr<std::vector<unsigned char>> AbstractCommunicator::SendMessage(const std::string &target_server_name,
                                                                              const void *data, size_t data_size,
                                                                              const std::string &http_uri_path,
                                                                              const std::string &target_msg_type,
                                                                              const std::string &offset) {
  if (data == nullptr) {
    MS_LOG(EXCEPTION) << "Data for sending request is nullptr.";
  }
  if (data_size == 0) {
    MS_LOG(EXCEPTION) << "Data size for sending request must be greater than 0";
  }
  if (remote_server_address_.count(target_server_name) <= 0 || http_clients_.count(target_server_name) <= 0) {
    for (auto item : remote_server_address_) {
      MS_LOG(ERROR) << "remote_server_address_ is:" << item.first << "-" << item.second;
    }
    for (auto item : http_clients_) {
      MS_LOG(ERROR) << "http_clients_ is:" << item.first;
    }
    MS_LOG(EXCEPTION) << "Remote server name is invalid. target_server_name is:" << target_server_name;
  }
  auto http_client = http_clients_[target_server_name];
  MS_EXCEPTION_IF_NULL(http_client);
  auto request_track = AddMessageTrack(1, nullptr);
  auto http_server_name = VFLContext::instance()->http_server_name();
  std::shared_ptr<std::vector<unsigned char>> response_msg;
  for (uint32_t i = 0; i < kRetryCommunicateTimes; i++) {
    if (!http_client->SendMessage(data, data_size, request_track, http_uri_path, target_msg_type, http_server_name,
                                  offset, HTTP_CONTENT_TYPE_URL_ENCODED)) {
      MS_LOG(WARNING) << "Sending request failed.";
    }
    if (!Wait(request_track)) {
      MS_LOG(WARNING) << "Sending http message timeout, now retry time is " << i;
      http_client->BreakLoopEvent();
    }
    response_msg = http_client->response_msg();
    if (!response_msg) {
      MS_LOG(WARNING) << "The http response message is invalid, now retry time is " << i;
    } else {
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(kSleepSecondsOfCommunicate));
  }
  if (!response_msg) {
    MS_LOG(EXCEPTION) << "Send data join message timeout for retry " << kRetryCommunicateTimes << " times.";
  }
  return response_msg;
}

void AbstractCommunicator::SendResponseMsg(const std::shared_ptr<MessageHandler> &message, const void *data,
                                           size_t len) {
  if (!verifyResponse(message, data, len)) {
    return;
  }
  if (!message->SendResponse(data, len, message->message_id())) {
    MS_LOG(WARNING) << "Sending response failed.";
    return;
  }
}

bool AbstractCommunicator::verifyResponse(const std::shared_ptr<MessageHandler> &message, const void *data,
                                          size_t len) {
  if (message == nullptr) {
    MS_LOG(WARNING) << "The message handler is nullptr.";
    return false;
  }
  if (data == nullptr || len == 0) {
    std::string reason = "The output of the msg is empty.";
    MS_LOG(WARNING) << reason;
    if (!message->SendResponse(reason.c_str(), reason.size())) {
      MS_LOG(WARNING) << "Sending response failed.";
    }
    return false;
  }
  return true;
}

std::string AbstractCommunicator::toString(ResponseElem elem) {
  switch (elem) {
    case ResponseElem::SUCCESS:
      return "SUCCESS";
    case ResponseElem::FAILED:
      return "FAILED";
    default:
      return "";
  }
  return "";
}
}  // namespace fl
}  // namespace mindspore
