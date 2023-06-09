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

#include <fstream>
#include <cstring>
#include <sstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <random>

#include "armour/util/io_util.h"
#include "armour/base_crypto/bloom_filter.h"
#include "vertical/utils/psi_utils.h"

namespace mindspore {
namespace fl {
namespace psi {
std::string ReadBinFile(const std::string &csv_path) {
  MS_LOG(INFO) << "Start read Bin file...";
  std::ifstream input(csv_path);
  std::ostringstream tmp;
  tmp << input.rdbuf();
  return tmp.str();
}

void WriteFile(const std::string &out_path, const std::string &ret) {
  MS_LOG(INFO) << "Start write Bin file: " << out_path;
  std::ofstream out_file;
  out_file.open(out_path);
  out_file << ret;
  MS_LOG(INFO) << "Start write filter over, file length is " << ret.length();
}

bool Send(const ClientPSIInit &client_psi_init) {
  std::shared_ptr<datajoin::ClientPSIInitProto> client_init_proto_ptr =
    std::make_shared<datajoin::ClientPSIInitProto>();
  CreateClientPSIInitProto(client_init_proto_ptr.get(), client_psi_init);
  std::string data = client_init_proto_ptr->SerializeAsString();
  size_t data_size = data.size();
  MS_LOG(INFO) << "Send client_psi_init data size is " << data_size;
  // write
  WriteFile("client_psi_init", data);
  return true;
}

void Recv(ClientPSIInit *client_psi_init) {
  if (client_psi_init == nullptr) {
    MS_LOG(ERROR) << "client_psi_init is null!";
    return;
  }
  std::string bin_array = ReadBinFile("client_psi_init");
  datajoin::ClientPSIInitProto client_init_proto;
  client_init_proto.ParseFromArray(bin_array.data(), static_cast<int>(bin_array.size()));
  client_psi_init->set_bin_id(client_init_proto.bin_id());
  client_psi_init->set_psi_type(client_init_proto.psi_type());
  client_psi_init->set_self_size(client_init_proto.self_size());
  MS_LOG(INFO) << "client_psi_init, bin_id is " << client_psi_init->bin_id();
  MS_LOG(INFO) << "client_psi_init, psi_type is " << client_psi_init->psi_type();
  MS_LOG(INFO) << "client_psi_init, self_size is " << client_psi_init->self_size();
}

bool Send(const ServerPSIInit &server_psi_init) {
  std::shared_ptr<datajoin::ServerPSIInitProto> server_init_proto_ptr =
    std::make_shared<datajoin::ServerPSIInitProto>();
  CreateServerPSIInitProto(server_init_proto_ptr.get(), server_psi_init);
  std::string data = server_init_proto_ptr->SerializeAsString();
  size_t data_size = data.size();
  MS_LOG(INFO) << "Send server_psi_init data size is " << data_size;
  WriteFile("server_psi_init", data);
  return true;
}

void Recv(ServerPSIInit *server_psi_init) {
  if (server_psi_init == nullptr) {
    MS_LOG(ERROR) << "server_psi_init is null!";
    return;
  }
  std::string bin_array = ReadBinFile("server_psi_init");
  datajoin::ServerPSIInitProto server_init_proto;
  server_init_proto.ParseFromArray(bin_array.data(), static_cast<int>(bin_array.size()));
  server_psi_init->set_bin_id(server_init_proto.bin_id());
  server_psi_init->set_self_size(server_init_proto.self_size());
  server_psi_init->set_self_role(server_init_proto.self_role());
  MS_LOG(INFO) << "server_psi_init, bin_id is " << server_psi_init->bin_id();
  MS_LOG(INFO) << "server_psi_init, self_size is " << server_psi_init->self_size();
  MS_LOG(INFO) << "server_psi_init, self_role is " << server_psi_init->self_role();
}

bool Send(const BobPb &bob_p_b) {
  std::shared_ptr<datajoin::BobPbProto> bob_p_b_proto_ptr = std::make_shared<datajoin::BobPbProto>();
  CreateBobPbProto(bob_p_b_proto_ptr.get(), bob_p_b);
  std::string data = bob_p_b_proto_ptr->SerializeAsString();
  size_t data_size = data.size();
  MS_LOG(INFO) << "Send bob_p_b data size is " << data_size;
  WriteFile("bob_p_b", data);
  return true;
}

void Recv(BobPb *bob_p_b) {
  if (bob_p_b == nullptr) {
    MS_LOG(ERROR) << "bob_p_b is null!";
    return;
  }
  std::string bin_array = ReadBinFile("bob_p_b");
  datajoin::BobPbProto bob_p_b_proto;
  bob_p_b_proto.ParseFromArray(bin_array.data(), static_cast<int>(bin_array.size()));
  bob_p_b->set_bin_id(bob_p_b_proto.bin_id());
  std::vector<std::string> p_b_vct;
  int p_b_vct_size = bob_p_b_proto.p_b_vct_size();
  for (int i = 0; i < p_b_vct_size; i++) {
    p_b_vct.push_back(bob_p_b_proto.p_b_vct(i));
  }
  bob_p_b->set_p_b_vct(p_b_vct);
  MS_LOG(INFO) << "bob_p_b, bin_id is " << bob_p_b->bin_id();
  MS_LOG(INFO) << "bob_p_b size is " << bob_p_b->p_b_vct().size();
}

bool Send(const AlicePbaAndBF &alice_pba_bf) {
  std::shared_ptr<datajoin::AlicePbaAndBFProto> alice_pba_bf_proto_ptr =
    std::make_shared<datajoin::AlicePbaAndBFProto>();
  CreateAlicePbaAndBFProto(alice_pba_bf_proto_ptr.get(), alice_pba_bf);
  std::string data = alice_pba_bf_proto_ptr->SerializeAsString();
  size_t data_size = data.size();
  MS_LOG(INFO) << "Send alice_pba_bf data size is " << data_size;
  WriteFile("alice_pba_bf", data);
  return true;
}

void Recv(AlicePbaAndBF *alice_p_b_a_bf) {
  if (alice_p_b_a_bf == nullptr) {
    MS_LOG(ERROR) << "alice_p_b_a_bf is null!";
    return;
  }
  std::string bin_array = ReadBinFile("alice_pba_bf");
  datajoin::AlicePbaAndBFProto alice_p_b_a_bf_proto;
  alice_p_b_a_bf_proto.ParseFromArray(bin_array.data(), static_cast<int>(bin_array.size()));
  alice_p_b_a_bf->set_bin_id(alice_p_b_a_bf_proto.bin_id());
  std::vector<std::string> p_b_vct;
  int p_b_a_vct_size = alice_p_b_a_bf_proto.p_b_a_vct_size();
  for (int i = 0; i < p_b_a_vct_size; i++) {
    p_b_vct.push_back(alice_p_b_a_bf_proto.p_b_a_vct(i));
  }
  alice_p_b_a_bf->set_p_b_a_vct(p_b_vct);
  alice_p_b_a_bf->set_bf_alice(alice_p_b_a_bf_proto.bf_alice());
  MS_LOG(INFO) << "alice_pba_bf, bin_id is " << alice_p_b_a_bf->bin_id();
  MS_LOG(INFO) << "alice_p_b_a size is " << alice_p_b_a_bf->p_b_a_vct().size();
  MS_LOG(INFO) << "bf_alice byte length is " << alice_p_b_a_bf->bf_alice().size();
}

bool Send(const BobAlignResult &bob_align_result) {
  std::shared_ptr<datajoin::BobAlignResultProto> alice_result_proto_ptr =
    std::make_shared<datajoin::BobAlignResultProto>();
  CreateBobAlignResultProto(alice_result_proto_ptr.get(), bob_align_result);
  std::string data = alice_result_proto_ptr->SerializeAsString();
  size_t data_size = data.size();
  MS_LOG(INFO) << "Send bob_align_result data size is " << data_size;
  WriteFile("bob_align_result", data);
  return true;
}

void Recv(BobAlignResult *bob_align_result) {
  if (bob_align_result == nullptr) {
    MS_LOG(ERROR) << "bob_align_result is null!";
    return;
  }
  std::string bin_array = ReadBinFile("bob_align_result");
  datajoin::BobAlignResultProto bob_align_result_proto;
  bob_align_result_proto.ParseFromArray(bin_array.data(), static_cast<int>(bin_array.size()));
  bob_align_result->set_bin_id(bob_align_result_proto.bin_id());
  std::vector<std::string> align_result;
  int align_result_size = bob_align_result_proto.align_result_size();
  for (int i = 0; i < align_result_size; i++) {
    align_result.push_back(bob_align_result_proto.align_result(i));
  }
  bob_align_result->set_align_result(align_result);
  MS_LOG(INFO) << "bob_align_result, bin_id is " << bob_align_result->bin_id();
  MS_LOG(INFO) << "bob_align_result size is " << bob_align_result->align_result().size();
}

bool Send(const AliceCheck &alice_check) {
  std::shared_ptr<datajoin::AliceCheckProto> alice_check_proto_ptr = std::make_shared<datajoin::AliceCheckProto>();
  CreateAliceCheckProto(alice_check_proto_ptr.get(), alice_check);
  std::string data = alice_check_proto_ptr->SerializeAsString();
  size_t data_size = data.size();
  MS_LOG(INFO) << "Send alice_check data size is " << data_size;
  WriteFile("alice_check", data);
  return true;
}

void Recv(AliceCheck *alice_check) {
  if (alice_check == nullptr) {
    MS_LOG(ERROR) << "alice_check is null!";
    return;
  }
  std::string bin_array = ReadBinFile("alice_check");
  datajoin::AliceCheckProto alice_check_proto;
  alice_check_proto.ParseFromArray(bin_array.data(), static_cast<int>(bin_array.size()));
  alice_check->set_bin_id(alice_check_proto.bin_id());
  alice_check->set_wrong_num(alice_check_proto.wrong_num());
  std::vector<std::string> wrong_id_vct;
  int wrong_id_size = alice_check_proto.wrong_id_size();
  for (int i = 0; i < wrong_id_size; i++) {
    wrong_id_vct.push_back(alice_check_proto.wrong_id(i));
  }
  alice_check->set_wrong_id(wrong_id_vct);
  MS_LOG(INFO) << "alice_check, bin_id is " << alice_check->bin_id();
  MS_LOG(INFO) << "alice_check, wrong_id size is " << alice_check->wrong_id().size();
}

}  // namespace psi
}  // namespace fl
}  // namespace mindspore
