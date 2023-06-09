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

#include "server/kernel/round/get_model_kernel.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "server/model_store.h"

namespace mindspore {
namespace fl {
namespace server {
namespace kernel {
void GetModelKernel::InitKernel(size_t) { InitClientVisitedNum(); }

bool GetModelKernel::Launch(const uint8_t *req_data, size_t len, const std::shared_ptr<MessageHandler> &message) {
  std::shared_ptr<FBBuilder> fbb = std::make_shared<FBBuilder>();
  if (fbb == nullptr || req_data == nullptr) {
    std::string reason = "FBBuilder builder or req_data is nullptr.";
    MS_LOG(ERROR) << reason;
    SendResponseMsg(message, reason.c_str(), reason.size());
    return true;
  }

  flatbuffers::Verifier verifier(req_data, len);
  if (!verifier.VerifyBuffer<schema::RequestGetModel>()) {
    std::string reason = "The schema of RequestGetModel is invalid.";
    BuildGetModelRsp(fbb, schema::ResponseCode_RequestError, reason, cache::InstanceContext::Instance().iteration_num(),
                     {}, "");
    MS_LOG(ERROR) << reason;
    SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
    return true;
  }

  retry_count_ += 1;
  if (retry_count_.load() % kPrintGetModelForEveryRetryTime == 1) {
    MS_LOG(DEBUG) << "Launching GetModelKernel kernel. Retry count is " << retry_count_.load();
  }

  const schema::RequestGetModel *get_model_req = flatbuffers::GetRoot<schema::RequestGetModel>(req_data);
  if (get_model_req == nullptr) {
    std::string reason = "Building flatbuffers schema failed for RequestGetModel.";
    MS_LOG(ERROR) << reason;
    SendResponseMsg(message, reason.c_str(), reason.size());
    return true;
  }
  GetModel(get_model_req, message);
  return true;
}

bool GetModelKernel::Reset() {
  MS_LOG(INFO) << "Get model kernel reset!";
  retry_count_ = 0;
  return true;
}

void GetModelKernel::GetModel(const schema::RequestGetModel *get_model_req,
                              const std::shared_ptr<MessageHandler> &message) {
  std::shared_ptr<FBBuilder> fbb = std::make_shared<FBBuilder>();
  if (fbb == nullptr) {
    std::string reason = "FBBuilder builder is nullptr.";
    MS_LOG(ERROR) << reason;
    SendResponseMsg(message, reason.c_str(), reason.size());
    return;
  }
  auto next_req_time = LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp);
  ModelItemPtr model_item = nullptr;
  size_t current_iter = cache::InstanceContext::Instance().iteration_num();
  size_t get_model_iter = IntToSize(get_model_req->iteration());
  const auto &iter_to_model = ModelStore::GetInstance().iteration_to_model();
  size_t model_latest_iter_num = iter_to_model.rbegin()->first;
  // If this iteration is not finished yet, return ResponseCode_SucNotReady so that clients could get model later.
  if (current_iter == get_model_iter && model_latest_iter_num != current_iter) {
    std::string reason = "The model is not ready yet for iteration " + std::to_string(get_model_iter) +
                         ". Maybe this is because\n" + "1. Client doesn't not send enough update model request.\n" +
                         "2. Worker has not push weights to server.";
    BuildGetModelRsp(fbb, schema::ResponseCode_SucNotReady, reason, current_iter, model_item,
                     std::to_string(next_req_time));
    if (retry_count_.load() % kPrintGetModelForEveryRetryTime == 1) {
      MS_LOG(DEBUG) << reason;
    }
    SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
    return;
  }

  IncreaseAcceptClientNum();
  auto real_get_model_iter = get_model_iter;
  if (iter_to_model.count(get_model_iter) == 0) {
    // If the model of get_model_iter is not stored, return the latest version of model and current iteration number.
    MS_LOG(DEBUG) << "The iteration of GetModel request " << std::to_string(get_model_iter)
                  << " is invalid. Current iteration is " << std::to_string(current_iter);
    real_get_model_iter = current_iter - 1;
  }
  auto download_compress_types = get_model_req->download_compress_types();
  schema::CompressType compressType =
    mindspore::fl::compression::CompressExecutor::GetInstance().GetCompressType(download_compress_types);
  std::string compress_type;
  if (compressType == schema::CompressType_QUANT) {
    compress_type = kQuant;
  } else {
    compress_type = kNoCompressType;
  }
  auto cache = ModelStore::GetInstance().GetModelResponseCache(name_, current_iter, real_get_model_iter, compress_type);
  if (cache == nullptr) {
    // Only download compress weights if client support.
    std::map<std::string, AddressPtr> compress_feature_maps = {};
    if (compressType == schema::CompressType_NO_COMPRESS) {
      model_item = ModelStore::GetInstance().GetModelByIterNum(real_get_model_iter);
      if (model_item == nullptr) {
        MS_LOG(WARNING) << "The feature map for getModel is empty.";
      }
    } else {
      auto &compressExecutor = mindspore::fl::compression::CompressExecutor::GetInstance();
      if (compressExecutor.EnableCompressWeight(compressType)) {
        compress_feature_maps = ModelStore::GetInstance().GetCompressModelByIterNum(real_get_model_iter, compressType);
      }
    }
    BuildGetModelRsp(fbb, schema::ResponseCode_SUCCEED, "Get model for iteration " + std::to_string(get_model_iter),
                     current_iter, model_item, std::to_string(next_req_time), compressType, compress_feature_maps);
    cache = ModelStore::GetInstance().StoreModelResponseCache(name_, current_iter, real_get_model_iter, compress_type,
                                                              fbb->GetBufferPointer(), fbb->GetSize());
    if (cache == nullptr) {
      SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
      return;
    }
  }
  SendResponseMsgInference(message, cache->data(), cache->size(), ModelStore::GetInstance().RelModelResponseCache);
  MS_LOG(DEBUG) << "GetModel last iteration is valid or not: "
                << cache::InstanceContext::Instance().last_iteration_valid() << ", next request time is "
                << next_req_time << ", current iteration is " << current_iter;
  return;
}

void GetModelKernel::BuildGetModelRsp(const std::shared_ptr<FBBuilder> &fbb, const schema::ResponseCode retcode,
                                      const std::string &reason, const size_t iter, const ModelItemPtr &model,
                                      const std::string &timestamp, const schema::CompressType &compressType,
                                      const std::map<std::string, AddressPtr> &compress_feature_maps) {
  if (fbb == nullptr) {
    MS_LOG(ERROR) << "Input fbb is nullptr.";
    return;
  }
  auto server_mode = FLContext::instance()->server_mode();
  auto aggregation_type = FLContext::instance()->aggregation_type();
  auto fbs_reason = fbb->CreateString(reason);
  auto fbs_timestamp = fbb->CreateString(timestamp);
  std::vector<flatbuffers::Offset<schema::FeatureMap>> fbs_feature_maps;
  if (model) {
    auto weight_data = model->weight_data.data();
    for (const auto &feature : model->weight_items) {
      auto weight_item = feature.second;
      bool flag1 = (!weight_item.require_aggr && server_mode != kServerModeHybrid);
      bool flag2 = (aggregation_type == kScaffoldAggregation && startswith(feature.first, kControlPrefix));
      if (flag1 || flag2) {
        continue;
      }
      auto fbs_weight_fullname = fbb->CreateString(feature.first);
      auto fbs_weight_data = fbb->CreateVector(reinterpret_cast<float *>(weight_data + weight_item.offset),
                                               weight_item.size / sizeof(float));
      auto fbs_feature_map = schema::CreateFeatureMap(*(fbb.get()), fbs_weight_fullname, fbs_weight_data);
      fbs_feature_maps.push_back(fbs_feature_map);
    }
  }
  auto fbs_feature_maps_vector = fbb->CreateVector(fbs_feature_maps);

  // construct compress feature maps with fbs
  std::vector<flatbuffers::Offset<schema::CompressFeatureMap>> fbs_compress_feature_maps;
  for (const auto &compress_feature_map : compress_feature_maps) {
    if (compress_feature_map.first.find(kMinVal) != std::string::npos ||
        compress_feature_map.first.find(kMaxVal) != std::string::npos) {
      continue;
    }
    auto fbs_compress_weight_fullname = fbb->CreateString(compress_feature_map.first);
    auto fbs_compress_weight_data =
      fbb->CreateVector(reinterpret_cast<const int8_t *>(compress_feature_map.second->addr),
                        compress_feature_map.second->size / sizeof(int8_t));

    const std::string min_val_name = compress_feature_map.first + "." + kMinVal;
    const std::string max_val_name = compress_feature_map.first + "." + kMaxVal;

    const AddressPtr min_val_ptr = compress_feature_maps.at(min_val_name);
    const AddressPtr max_val_ptr = compress_feature_maps.at(max_val_name);

    auto fbs_min_val_ptr = reinterpret_cast<const float *>(min_val_ptr->addr);
    auto fbs_max_val_ptr = reinterpret_cast<const float *>(max_val_ptr->addr);
    auto fbs_compress_feature_map = schema::CreateCompressFeatureMap(
      *(fbb.get()), fbs_compress_weight_fullname, fbs_compress_weight_data, *fbs_min_val_ptr, *fbs_max_val_ptr);

    fbs_compress_feature_maps.push_back(fbs_compress_feature_map);
  }
  auto fbs_compress_feature_maps_vector = fbb->CreateVector(fbs_compress_feature_maps);

  schema::ResponseGetModelBuilder rsp_get_model_builder(*(fbb.get()));
  rsp_get_model_builder.add_retcode(static_cast<int>(retcode));
  rsp_get_model_builder.add_reason(fbs_reason);
  rsp_get_model_builder.add_iteration(static_cast<int>(iter));
  rsp_get_model_builder.add_feature_map(fbs_feature_maps_vector);
  rsp_get_model_builder.add_timestamp(fbs_timestamp);
  rsp_get_model_builder.add_download_compress_type(compressType);
  rsp_get_model_builder.add_compress_feature_map(fbs_compress_feature_maps_vector);
  auto rsp_get_model = rsp_get_model_builder.Finish();
  fbb->Finish(rsp_get_model);
  return;
}

REG_ROUND_KERNEL(getModel, GetModelKernel)
}  // namespace kernel
}  // namespace server
}  // namespace fl
}  // namespace mindspore
