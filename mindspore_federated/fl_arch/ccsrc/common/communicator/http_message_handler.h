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

#ifndef MINDSPORE_CCSRC_FL_COMMUNICATOR_HTTP_MESSAGE_HANDLER_H_
#define MINDSPORE_CCSRC_FL_COMMUNICATOR_HTTP_MESSAGE_HANDLER_H_

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/listener.h>
#include <event2/util.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "common/core/comm_util.h"
#include "common/utils/log_adapter.h"
#include "nlohmann/json.hpp"
#include "common/constants.h"
#include "common/status.h"

namespace mindspore {
namespace fl {
using HttpHeaders = std::map<std::string, std::list<std::string>>;

class HttpMessageHandler {
 public:
  HttpMessageHandler()
      : event_request_(nullptr),
        event_uri_(nullptr),
        path_params_{0},
        head_params_(nullptr),
        post_params_{0},
        post_param_parsed_(false),
        post_message_(nullptr),
        body_(nullptr),
        resp_headers_(nullptr),
        resp_buf_(nullptr),
        resp_code_(HTTP_OK),
        content_len_(0),
        event_base_(nullptr),
        offset_(0) {}

  virtual ~HttpMessageHandler() = default;

  void InitHttpMessage();

  std::string GetRequestUri() const;
  std::string GetRequestHost();
  const char *GetHostByUri() const;
  std::string GetHeadParam(const std::string &key) const;
  std::string GetPathParam(const std::string &key) const;
  std::string GetPostParam(const std::string &key);
  bool GetPostMsg(size_t *len, void **buffer);
  std::string GetUriPath() const;
  std::string GetRequestPath();
  std::string GetUriQuery() const;

  // It will return -1 if no port set
  int GetUriPort() const;

  // Useless to get from a request url, fragment is only for browser to locate sth.
  std::string GetUriFragment() const;

  void AddRespHeadParam(const std::string &key, const std::string &val);
  void AddRespHeaders(const HttpHeaders &headers);
  void AddRespString(const std::string &str);
  void SetRespCode(int code);

  // Make sure code and all response body has finished set
  void SendResponse();
  void QuickResponse(int code, const void *body, size_t len);
  void QuickResponseInference(int code, const void *body, size_t len, evbuffer_ref_cleanup_cb cb);
  void SimpleResponse(int code, const HttpHeaders &headers, const std::string &body);
  void ErrorResponse(int code, const FlStatus &status);
  void ErrorResponse(int code, const std::string &error_msg);

  // If message is empty, libevent will use default error code message instead
  void RespError(int nCode, const std::string &message);
  // Body length should no more than MAX_POST_BODY_LEN, default 64kB
  void ParsePostParam();
  FlStatus ParsePostMessageToJson();
  void ReceiveMessage(const void *buffer, size_t num);
  void set_content_len(const uint64_t &len);
  uint64_t content_len() const;
  const event_base *http_base() const;
  void set_http_base(const struct event_base *base);
  void set_request(const struct evhttp_request *req);
  const struct evhttp_request *request() const;
  void InitBodySize();
  std::shared_ptr<std::vector<char>> body();
  void set_body(const std::shared_ptr<std::vector<char>> &body);
  nlohmann::json request_message() const;
  FlStatus ParseValueFromKey(const std::string &key, uint32_t *const value);

  // Parse node ids when receiving an http request for scale in
  FlStatus ParseNodeIdsFromKey(const std::string &key, std::vector<std::string> *const value);

 private:
  struct evhttp_request *event_request_;
  const struct evhttp_uri *event_uri_;
  struct evkeyvalq path_params_;
  struct evkeyvalq *head_params_;
  struct evkeyvalq post_params_;
  bool post_param_parsed_;
  std::unique_ptr<std::string> post_message_;
  std::shared_ptr<std::vector<char>> body_;
  struct evkeyvalq *resp_headers_;
  struct evbuffer *resp_buf_;
  int resp_code_;
  uint64_t content_len_;
  struct event_base *event_base_;
  uint64_t offset_;
  nlohmann::json request_message_;
};
}  // namespace fl
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_FL_COMMUNICATOR_HTTP_MESSAGE_HANDLER_H_
