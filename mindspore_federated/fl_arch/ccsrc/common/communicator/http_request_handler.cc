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

#include "common/communicator/http_request_handler.h"
#include <vector>

namespace mindspore {
namespace fl {
HttpRequestHandler::~HttpRequestHandler() {
  if (evbase_) {
    event_base_free(evbase_);
    evbase_ = nullptr;
  }
}

bool HttpRequestHandler::Initialize(int fd, const std::unordered_map<std::string, OnRequestReceive *> &handlers) {
  evbase_ = event_base_new();
  MS_EXCEPTION_IF_NULL(evbase_);
  struct evhttp *http = evhttp_new(evbase_);
  MS_EXCEPTION_IF_NULL(http);

  if (FLContext::instance()->enable_ssl()) {
    MS_LOG(INFO) << "Enable ssl support.";
    auto ssl_ctx = SSLHTTP::GetInstance().GetSSLCtx();
    MS_EXCEPTION_IF_NULL(ssl_ctx);
    if (!SSL_CTX_set_options(ssl_ctx, SSL_OP_SINGLE_DH_USE | SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_SSLv2 |
                                        SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1)) {
      if (evbase_) {
        event_base_free(evbase_);
        evbase_ = nullptr;
      }
      evhttp_free(http);
      http = nullptr;
      MS_LOG(EXCEPTION) << "SSL_CTX_set_options failed.";
    }
    evhttp_set_bevcb(http, BuffereventCallback, ssl_ctx);
  }

  int result = evhttp_accept_socket(http, fd);
  if (result < 0) {
    MS_LOG(ERROR) << "Evhttp accept socket failed!";
    return false;
  }
  std::vector<std::string> addresses;
  for (const auto &handler : handlers) {
    auto TransFunc = [](struct evhttp_request *req, void *arg) {
      try {
        MS_EXCEPTION_IF_NULL(req);
        MS_EXCEPTION_IF_NULL(arg);
        auto httpReq = std::make_shared<HttpMessageHandler>();
        MS_EXCEPTION_IF_NULL(httpReq);
        httpReq->set_request(req);
        httpReq->InitHttpMessage();
        OnRequestReceive *func = reinterpret_cast<OnRequestReceive *>(arg);
        MS_EXCEPTION_IF_NULL(func);
        (*func)(httpReq);
      } catch (const std::exception &e) {
        MS_LOG(ERROR) << "Catch exception: " << e.what();
      }
    };

    // O SUCCESS,-1 ALREADY_EXIST,-2 FAILURE
    MS_EXCEPTION_IF_NULL(handler.second);
    int ret = evhttp_set_cb(http, handler.first.c_str(), TransFunc, reinterpret_cast<void *>(handler.second));
    if (ret == 0) {
      addresses.push_back(handler.first);
    } else if (ret == -1) {
      MS_LOG(WARNING) << "Ev http register handle of: " << handler.first << " exist.";
    } else {
      MS_LOG(ERROR) << "Ev http register handle of: " << handler.first << " failed.";
      return false;
    }
  }
  MS_LOG(INFO) << "Ev http register handle of: " << addresses << " success.";
  return true;
}

void HttpRequestHandler::Run() {
  MS_LOG(INFO) << "Start http server!";
  MS_EXCEPTION_IF_NULL(evbase_);
  int ret = event_base_dispatch(evbase_);
  if (ret == 1) {
    MS_LOG(ERROR) << "Event base dispatch failed with no events pending or active!";
  } else if (ret == -1) {
    MS_LOG(ERROR) << "Event base dispatch failed with error occurred!";
  } else if (ret != 0) {
    MS_LOG(ERROR) << "Event base dispatch with unexpected error code " << ret;
  }
}

bool HttpRequestHandler::Stop() {
  MS_EXCEPTION_IF_NULL(evbase_);
  if (event_base_got_break(evbase_)) {
    MS_LOG(INFO) << "The event base has already been stopped!";
    return true;
  }
  int ret = event_base_loopbreak(evbase_);
  if (ret != 0) {
    MS_LOG(ERROR) << "event base loop break failed!";
    return false;
  }
  return true;
}

bufferevent *HttpRequestHandler::BuffereventCallback(event_base *base, void *arg) {
  MS_EXCEPTION_IF_NULL(base);
  MS_EXCEPTION_IF_NULL(arg);
  SSL_CTX *ctx = reinterpret_cast<SSL_CTX *>(arg);
  SSL *ssl = SSL_new(ctx);
  MS_EXCEPTION_IF_NULL(ssl);
  bufferevent *bev = bufferevent_openssl_socket_new(base, -1, ssl, BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
  MS_EXCEPTION_IF_NULL(bev);
  return bev;
}
}  // namespace fl
}  // namespace mindspore
