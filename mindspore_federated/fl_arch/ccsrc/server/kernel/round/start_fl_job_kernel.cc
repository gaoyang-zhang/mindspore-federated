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

#include "server/kernel/round/start_fl_job_kernel.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "armour/cipher/cipher_init.h"
#include "server/model_store.h"
#include "server/iteration.h"
#include "distributed_cache/timer.h"

namespace mindspore {
namespace fl {
namespace server {
namespace kernel {
void StartFLJobKernel::InitKernel(size_t) {
  iter_next_req_timestamp_ = LongToUlong(CURRENT_TIME_MILLI.count()) + iteration_time_window();
  LocalMetaStore::GetInstance().put_value(kCtxIterationNextRequestTimestamp, iter_next_req_timestamp_);
  InitClientVisitedNum();
}

bool StartFLJobKernel::VerifyFLJobRequest(const schema::RequestFLJob *start_fl_job_req) {
  MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req, false);
  MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req->fl_id(), false);
  MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req->fl_name(), false);
  MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req->timestamp(), false);

  if (FLContext::instance()->pki_verify()) {
    MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req->key_attestation(), false);
    MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req->equip_cert(), false);
    MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req->equip_ca_cert(), false);
    MS_ERROR_IF_NULL_W_RET_VAL(start_fl_job_req->sign_data(), false);
  }
  return true;
}

bool StartFLJobKernel::Launch(const uint8_t *req_data, size_t len, const std::shared_ptr<MessageHandler> &message) {
  MS_LOG(DEBUG) << "Launching StartFLJobKernel kernel.";
  std::shared_ptr<FBBuilder> fbb = std::make_shared<FBBuilder>();
  if (fbb == nullptr || req_data == nullptr) {
    std::string reason = "FBBuilder builder or req_data is nullptr.";
    MS_LOG(WARNING) << reason;
    SendResponseMsg(message, reason.c_str(), reason.size());
    return false;
  }

  flatbuffers::Verifier verifier(req_data, len);
  if (!verifier.VerifyBuffer<schema::RequestFLJob>()) {
    std::string reason = "The schema of RequestFLJob is invalid.";
    BuildStartFLJobRsp(fbb, schema::ResponseCode_RequestError, reason, false, "");
    MS_LOG(WARNING) << reason;
    SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
    return false;
  }

  const schema::RequestFLJob *start_fl_job_req = flatbuffers::GetRoot<schema::RequestFLJob>(req_data);
  if (!VerifyFLJobRequest(start_fl_job_req)) {
    std::string reason = "Verify flatbuffers schema failed for RequestFLJob.";
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_RequestError, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    MS_LOG(WARNING) << reason;
    SendResponseMsg(message, reason.c_str(), reason.size());
    return false;
  }

  ResultCode result_code = ReachThresholdForStartFLJob(fbb);
  if (result_code != ResultCode::kSuccess) {
    SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
    return false;
  }

  if (FLContext::instance()->pki_verify()) {
    if (!JudgeFLJobCert(fbb, start_fl_job_req)) {
      SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
      return false;
    }
    if (!StoreKeyAttestation(fbb, start_fl_job_req)) {
      SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
      return false;
    }
  }

  DeviceMeta device_meta = CreateDeviceMetadata(start_fl_job_req);
  uint64_t start_fl_job_time =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  device_meta.set_now_time(start_fl_job_time);
  result_code = ReadyForStartFLJob(fbb, device_meta);
  if (result_code != ResultCode::kSuccess) {
    SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
    return false;
  }
  auto fl_id = start_fl_job_req->fl_id()->c_str();
  auto ret = cache::ClientInfos::GetInstance().AddDeviceMeta(fl_id, device_meta);
  if (!ret.IsSuccess()) {
    std::string reason = "Updating device metadata failed for fl id " + device_meta.fl_id();
    MS_LOG(WARNING) << reason;
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_OutOfTime, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
    return false;
  }

  // If calling ReportCount before ReadyForStartFLJob, the result will be inconsistent if the device is not selected.
  result_code = CountForStartFLJob(fbb, start_fl_job_req);
  if (result_code != ResultCode::kSuccess) {
    SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
    return false;
  }
  IncreaseAcceptClientNum();
  auto curr_iter_num = cache::InstanceContext::Instance().iteration_num();
  auto last_iteration = curr_iter_num - 1;
  auto download_compress_types = start_fl_job_req->download_compress_types();
  schema::CompressType compressType =
    mindspore::fl::compression::CompressExecutor::GetInstance().GetCompressType(download_compress_types);
  std::string compress_type;
  if (compressType == schema::CompressType_QUANT) {
    compress_type = kQuant;
  } else {
    compress_type = kNoCompressType;
  }
  auto cache = ModelStore::GetInstance().GetModelResponseCache(name_, curr_iter_num, last_iteration, compress_type);
  if (cache == nullptr) {
    StartFLJob(fbb, device_meta, start_fl_job_req);
    cache = ModelStore::GetInstance().StoreModelResponseCache(name_, curr_iter_num, last_iteration, compress_type,
                                                              fbb->GetBufferPointer(), fbb->GetSize());
    if (cache == nullptr) {
      SendResponseMsg(message, fbb->GetBufferPointer(), fbb->GetSize());
      return false;
    }
  }
  SendResponseMsgInference(message, cache->data(), cache->size(), ModelStore::GetInstance().RelModelResponseCache);
  return true;
}

bool StartFLJobKernel::JudgeFLJobCert(const std::shared_ptr<FBBuilder> &fbb,
                                      const schema::RequestFLJob *start_fl_job_req) {
  std::string fl_id = start_fl_job_req->fl_id()->str();
  std::string timestamp = start_fl_job_req->timestamp()->str();
  auto sign_data_vector = start_fl_job_req->sign_data();
  if (sign_data_vector->size() == 0) {
    std::string reason = "sign data is empty.";
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_RequestError, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    MS_LOG(WARNING) << reason;
    return false;
  }
  unsigned char sign_data[sign_data_vector->size()];

  for (unsigned int i = 0; i < sign_data_vector->size(); i++) {
    sign_data[i] = sign_data_vector->Get(i);
  }

  std::string key_attestation = start_fl_job_req->key_attestation()->str();
  std::string equip_cert = start_fl_job_req->equip_cert()->str();
  std::string equip_ca_cert = start_fl_job_req->equip_ca_cert()->str();

  auto &client_verify_config = FLContext::instance()->client_verify_config();
  std::string root_first_ca_path = client_verify_config.root_first_ca_path;
  std::string root_second_ca_path = client_verify_config.root_second_ca_path;
  std::string equip_crl_path = client_verify_config.equip_crl_path;

  auto &certVerify = CertVerify::GetInstance();
  bool ret =
    certVerify.verifyCertAndSign(fl_id, timestamp, (const unsigned char *)sign_data, key_attestation, equip_cert,
                                 equip_ca_cert, root_first_ca_path, root_second_ca_path, equip_crl_path);
  if (!ret) {
    std::string reason = "startFLJob sign and certificate verify failed.";
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_RequestError, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    MS_LOG(WARNING) << reason;
  } else {
    MS_LOG(DEBUG) << "JudgeFLJobVerify success." << ret;
  }

  return ret;
}

bool StartFLJobKernel::StoreKeyAttestation(const std::shared_ptr<FBBuilder> &fbb,
                                           const schema::RequestFLJob *start_fl_job_req) {
  std::string fl_id = start_fl_job_req->fl_id()->str();
  std::string key_attestation = start_fl_job_req->key_attestation()->str();

  auto ret = cache::ClientInfos::GetInstance().AddClientKeyAttestation(fl_id, key_attestation);
  if (!ret.IsSuccess()) {
    std::string reason = "startFLJob: store key attestation failed";
    MS_LOG(WARNING) << reason;
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_OutOfTime, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    return false;
  }
  return true;
}

bool StartFLJobKernel::Reset() {
  MS_LOG(INFO) << "Starting fl job kernel reset!";
  cache::Timer::Instance().StopTimer(name_);
  return true;
}

void StartFLJobKernel::OnFirstCountEvent() {
  iter_next_req_timestamp_ = LongToUlong(CURRENT_TIME_MILLI.count()) + iteration_time_window();
  LocalMetaStore::GetInstance().put_value(kCtxIterationNextRequestTimestamp, iter_next_req_timestamp_);
  // The first startFLJob request means a new iteration starts running.
  Iteration::GetInstance().SetIterationRunning();
}

ResultCode StartFLJobKernel::ReachThresholdForStartFLJob(const std::shared_ptr<FBBuilder> &fbb) {
  if (DistributedCountService::GetInstance().CountReachThreshold(name_)) {
    std::string reason = "Current amount for startFLJob has reached the threshold. Please startFLJob later.";
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_OutOfTime, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    MS_LOG(DEBUG) << reason;
    return ResultCode::kFail;
  }
  return ResultCode::kSuccess;
}

DeviceMeta StartFLJobKernel::CreateDeviceMetadata(const schema::RequestFLJob *start_fl_job_req) {
  std::string fl_name = start_fl_job_req->fl_name()->str();
  std::string fl_id = start_fl_job_req->fl_id()->str();
  auto data_size = start_fl_job_req->data_size();
  auto eval_data_size = start_fl_job_req->eval_data_size();
  MS_LOG(DEBUG) << "DeviceMeta fl_name:" << fl_name << ", fl_id:" << fl_id << ", data_size:" << data_size
                << ", eval_data_size:" << eval_data_size;

  DeviceMeta device_meta;
  device_meta.set_fl_name(fl_name);
  device_meta.set_fl_id(fl_id);
  device_meta.set_data_size(data_size);
  device_meta.set_eval_data_size(eval_data_size);
  return device_meta;
}

ResultCode StartFLJobKernel::ReadyForStartFLJob(const std::shared_ptr<FBBuilder> &fbb, const DeviceMeta &device_meta) {
  ResultCode ret = ResultCode::kSuccess;
  std::string reason = "";
  if (device_meta.data_size() < 1) {
    reason = "FL job data size is not enough.";
    ret = ResultCode::kFail;
  }
  if (device_meta.data_size() > UINT16_MAX) {
    reason = "FL job data size is too large.";
    ret = ResultCode::kFail;
  }
  if (ret != ResultCode::kSuccess) {
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_OutOfTime, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    MS_LOG(DEBUG) << reason;
  }
  return ret;
}

ResultCode StartFLJobKernel::CountForStartFLJob(const std::shared_ptr<FBBuilder> &fbb,
                                                const schema::RequestFLJob *start_fl_job_req) {
  if (!DistributedCountService::GetInstance().Count(name_)) {
    std::string reason =
      "Counting start fl job request failed for fl id " + start_fl_job_req->fl_id()->str() + ", Please retry later.";
    BuildStartFLJobRsp(
      fbb, schema::ResponseCode_OutOfTime, reason, false,
      std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)));
    MS_LOG(WARNING) << reason;
    return ResultCode::kFail;
  }
  return ResultCode::kSuccess;
}

void StartFLJobKernel::StartFLJob(const std::shared_ptr<FBBuilder> &fbb, const DeviceMeta &,
                                  const schema::RequestFLJob *start_fl_job_req) {
  size_t last_iteration = cache::InstanceContext::Instance().iteration_num() - 1;

  ModelItemPtr model_item = nullptr;
  std::map<std::string, AddressPtr> compress_feature_maps = {};

  // Only download compress weights if client support.
  auto download_compress_types = start_fl_job_req->download_compress_types();
  schema::CompressType compressType =
    mindspore::fl::compression::CompressExecutor::GetInstance().GetCompressType(download_compress_types);
  if (compressType == schema::CompressType_NO_COMPRESS) {
    model_item = ModelStore::GetInstance().GetModelByIterNum(last_iteration);
    if (model_item == nullptr) {
      MS_LOG(WARNING) << "The feature map for startFLJob is empty, latest iteration num: " << last_iteration;
    }
  } else {
    if (mindspore::fl::compression::CompressExecutor::GetInstance().EnableCompressWeight(compressType)) {
      compress_feature_maps = ModelStore::GetInstance().GetCompressModelByIterNum(last_iteration, compressType);
    }
  }

  BuildStartFLJobRsp(fbb, schema::ResponseCode_SUCCEED, "success", true,
                     std::to_string(LocalMetaStore::GetInstance().value<uint64_t>(kCtxIterationNextRequestTimestamp)),
                     model_item, compressType, compress_feature_maps);
  return;
}

std::vector<flatbuffers::Offset<schema::FeatureMap>> StartFLJobKernel::BuildParamsRsp(
  const ModelItemPtr &model_item, const std::string &server_mode, const std::shared_ptr<FBBuilder> &fbb) {
  auto aggregation_type = FLContext::instance()->aggregation_type();
  std::vector<flatbuffers::Offset<schema::FeatureMap>> fbs_feature_maps;
  // No need to send weights in cross silo mode if aggregation type is not Scaffold during start fl job stage
  if (model_item) {
    auto weight_data_base = model_item->weight_data.data();
    for (auto &feature : model_item->weight_items) {
      bool flag1 = (server_mode == kServerModeHybrid || server_mode == kServerModeFL);
      bool flag2 = (aggregation_type == kScaffoldAggregation && startswith(feature.first, kControlPrefix));
      if (flag1 || flag2) {
        auto fbs_weight_fullname = fbb->CreateString(feature.first);
        auto fbs_weight_data = fbb->CreateVector(reinterpret_cast<float *>(weight_data_base + feature.second.offset),
                                                 feature.second.size / sizeof(float));
        auto fbs_feature_map = schema::CreateFeatureMap(*(fbb.get()), fbs_weight_fullname, fbs_weight_data);
        fbs_feature_maps.push_back(fbs_feature_map);
      }
    }
  }
  return fbs_feature_maps;
}

void StartFLJobKernel::BuildStartFLJobRsp(const std::shared_ptr<FBBuilder> &fbb, const schema::ResponseCode retcode,
                                          const std::string &reason, const bool is_selected,
                                          const std::string &next_req_time, const ModelItemPtr &model_item,
                                          const schema::CompressType &compressType,
                                          const std::map<std::string, AddressPtr> &compress_feature_maps) {
  if (fbb == nullptr) {
    MS_LOG(WARNING) << "Input fbb is nullptr.";
    return;
  }
  auto server_mode = FLContext::instance()->server_mode();
  auto fbs_reason = fbb->CreateString(reason);
  auto fbs_next_req_time = fbb->CreateString(next_req_time);
  auto fbs_server_mode = fbb->CreateString(server_mode);
  auto fbs_fl_name = fbb->CreateString(FLContext::instance()->fl_name());

  auto *param = armour::CipherInit::GetInstance().GetPublicParams();
  auto prime = fbb->CreateVector(param->prime, PRIME_MAX_LEN);
  auto p = fbb->CreateVector(param->p, SECRET_MAX_LEN);
  int32_t t = param->t;
  int32_t g = param->g;
  float dp_eps = param->dp_eps;
  float dp_delta = param->dp_delta;
  float dp_norm_clip = param->dp_norm_clip;
  auto encrypt_type = fbb->CreateString(FLContext::instance()->encrypt_type());
  float sign_k = param->sign_k;
  float sign_eps = param->sign_eps;
  float sign_thr_ratio = param->sign_thr_ratio;
  float sign_global_lr = param->sign_global_lr;
  int sign_dim_out = param->sign_dim_out;
  auto privacy_eval_type = param->privacy_eval_type;
  float laplace_eval_eps = param->laplace_eval_eps;

  auto pw_params = schema::CreatePWParams(*fbb.get(), t, p, g, prime);
  auto dp_params = schema::CreateDPParams(*fbb.get(), dp_eps, dp_delta, dp_norm_clip);
  auto ds_params = schema::CreateDSParams(*fbb.get(), sign_k, sign_eps, sign_thr_ratio, sign_global_lr, sign_dim_out);
  auto cipher_public_params =
    schema::CreateCipherPublicParams(*fbb.get(), encrypt_type, pw_params, dp_params, ds_params, laplace_eval_eps);

  schema::CompressType upload_compress_type;
  if (FLContext::instance()->compression_config().upload_compress_type == kDiffSparseQuant) {
    upload_compress_type = schema::CompressType_DIFF_SPARSE_QUANT;
  } else {
    upload_compress_type = schema::CompressType_NO_COMPRESS;
  }

  std::string unsupervised_eval_flg;
  auto eval_type = FLContext::instance()->unsupervised_config().eval_type;
  // There are only three cases of unsupervised_eval_flg: kNotEvalType, eval with no encryption, and eval with
  // encryption.
  if (eval_type == kNotEvalType) {
    unsupervised_eval_flg = kNotEvalType;
  } else {
    unsupervised_eval_flg = param->privacy_eval_type;
  }
  auto fbs_unsupervised_eval_flg = fbb->CreateString(unsupervised_eval_flg);
  schema::FLPlanBuilder fl_plan_builder(*(fbb.get()));
  fl_plan_builder.add_fl_name(fbs_fl_name);
  fl_plan_builder.add_server_mode(fbs_server_mode);
  fl_plan_builder.add_iterations(SizeToInt(FLContext::instance()->fl_iteration_num()));
  fl_plan_builder.add_epochs(SizeToInt(FLContext::instance()->client_epoch_num()));
  fl_plan_builder.add_mini_batch(SizeToInt(FLContext::instance()->client_batch_size()));
  fl_plan_builder.add_lr(FLContext::instance()->client_learning_rate());
  fl_plan_builder.add_cipher(cipher_public_params);

  auto fbs_fl_plan = fl_plan_builder.Finish();

  std::vector<flatbuffers::Offset<schema::FeatureMap>> fbs_feature_maps = BuildParamsRsp(model_item, server_mode, fbb);
  auto fbs_feature_maps_vector = fbb->CreateVector(fbs_feature_maps);

  // construct compress feature maps with fbs
  std::vector<flatbuffers::Offset<schema::CompressFeatureMap>> fbs_compress_feature_maps;
  for (const auto &compress_feature_map : compress_feature_maps) {
    if (compressType == schema::CompressType_QUANT) {
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
  }
  auto fbs_compress_feature_maps_vector = fbb->CreateVector(fbs_compress_feature_maps);

  schema::ResponseFLJobBuilder rsp_fl_job_builder(*(fbb.get()));
  rsp_fl_job_builder.add_retcode(static_cast<int>(retcode));
  rsp_fl_job_builder.add_reason(fbs_reason);
  rsp_fl_job_builder.add_iteration(SizeToInt(cache::InstanceContext::Instance().iteration_num()));
  rsp_fl_job_builder.add_is_selected(is_selected);
  rsp_fl_job_builder.add_next_req_time(fbs_next_req_time);
  rsp_fl_job_builder.add_fl_plan_config(fbs_fl_plan);
  rsp_fl_job_builder.add_feature_map(fbs_feature_maps_vector);
  rsp_fl_job_builder.add_download_compress_type(compressType);
  rsp_fl_job_builder.add_compress_feature_map(fbs_compress_feature_maps_vector);
  rsp_fl_job_builder.add_upload_compress_type(upload_compress_type);
  rsp_fl_job_builder.add_upload_sparse_rate(FLContext::instance()->compression_config().upload_sparse_rate);
  rsp_fl_job_builder.add_unsupervised_eval_flg(fbs_unsupervised_eval_flg);
  auto rsp_fl_job = rsp_fl_job_builder.Finish();
  fbb->Finish(rsp_fl_job);
  return;
}

REG_ROUND_KERNEL(startFLJob, StartFLJobKernel)
}  // namespace kernel
}  // namespace server
}  // namespace fl
}  // namespace mindspore
