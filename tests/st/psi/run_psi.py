# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""psi with communication st"""

import argparse
import ast
from mindspore_federated import VerticalFederatedCommunicator, ServerConfig
from mindspore_federated._mindspore_federated import RunPSI
from mindspore_federated._mindspore_federated import PlainIntersection
from mindspore_federated.startup.ssl_config import SSLConfig


def get_parser():
    """parser argument"""
    parser = argparse.ArgumentParser(description="Run PSI with Communication")

    parser.add_argument("--comm_role", type=str, default="server")
    parser.add_argument("--peer_comm_role", type=str, default="null")
    parser.add_argument("--http_server_address", type=str, default="127.0.0.1:8004")
    parser.add_argument("--remote_server_address", type=str, default="127.0.0.1:8005")
    parser.add_argument("--input_begin", type=int, default=1)
    parser.add_argument("--input_end", type=int, default=1000)
    parser.add_argument("--peer_input_begin", type=int, default=1)
    parser.add_argument("--peer_input_end", type=int, default=1000)
    parser.add_argument("--read_file", type=ast.literal_eval, default=False)
    parser.add_argument("--file_name", type=str, default="null")
    parser.add_argument("--peer_read_file", type=ast.literal_eval, default=False)
    parser.add_argument("--peer_file_name", type=str, default="null")
    parser.add_argument("--bucket_size", type=int, default=1)
    parser.add_argument("--thread_num", type=int, default=0)
    parser.add_argument("--need_check", type=ast.literal_eval, default=False)
    parser.add_argument("--plain_intersection", type=ast.literal_eval, default=False)
    parser.add_argument("--enable_ssl", type=ast.literal_eval, default=False)
    parser.add_argument("--server_password", type=str, default="")
    parser.add_argument("--client_password", type=str, default="")
    parser.add_argument("--server_cert_path", type=str, default="")
    parser.add_argument("--client_cert_path", type=str, default="")
    parser.add_argument("--ca_cert_path", type=str, default="")
    return parser


args, _ = get_parser().parse_known_args()

comm_role = args.comm_role
peer_comm_role = args.peer_comm_role
http_server_address = args.http_server_address
remote_server_address = args.remote_server_address
input_begin = args.input_begin
input_end = args.input_end
peer_input_begin = args.peer_input_begin
peer_input_end = args.peer_input_end
read_file = args.read_file
file_name = args.file_name
peer_read_file = args.peer_read_file
peer_file_name = args.peer_file_name
bucket_size = args.bucket_size
thread_num = args.thread_num
need_check = args.need_check
plain_intersection = args.plain_intersection
enable_ssl = args.enable_ssl
server_password = args.server_password
client_password = args.client_password
server_cert_path = args.server_cert_path
client_cert_path = args.client_cert_path
ca_cert_path = args.ca_cert_path


def compute_right_result(self_input, peer_input):
    self_input_set = set(self_input)
    peer_input_set = set(peer_input)
    return self_input_set.intersection(peer_input_set)


def check_psi(actual_result_, psi_result_):
    actual_result_set = set(actual_result_)
    psi_result_set = set(psi_result_)
    wrong_num = len(psi_result_set.difference(actual_result_set).union(actual_result_set.difference(psi_result_set)))
    return wrong_num


def generate_input_data(input_begin_, input_end_, read_file_, file_name_):
    input_data_ = []
    if read_file_:
        with open(file_name_, 'r') as f:
            for line in f.readlines():
                input_data_.append(line.strip())
    else:
        input_data_ = [str(i) for i in range(input_begin_, input_end_)]
    return input_data_


if __name__ == "__main__":
    if peer_comm_role == "null":
        role = ["server", "client"]
        peer_comm_role = role[1 - role.index(comm_role)]

    http_server_config = ServerConfig(server_name=comm_role, server_address=http_server_address)
    remote_server_config = ServerConfig(server_name=peer_comm_role, server_address=remote_server_address)
    # support ssl communication
    if enable_ssl:
        ssl_config = SSLConfig(server_password=server_password, client_password=client_password,
                               server_cert_path=server_cert_path,
                               client_cert_path=client_cert_path,
                               ca_cert_path=ca_cert_path)

        vertical_communicator = VerticalFederatedCommunicator(http_server_config=http_server_config,
                                                              remote_server_config=remote_server_config,
                                                              enable_ssl=True,
                                                              ssl_config=ssl_config)
    else:

        vertical_communicator = VerticalFederatedCommunicator(http_server_config=http_server_config,
                                                              remote_server_config=remote_server_config)

    vertical_communicator.launch()
    input_data = generate_input_data(input_begin, input_end, read_file, file_name)
    for bucket_id in range(bucket_size):
        if plain_intersection:
            intersection_type = "Plain intersection"
            intersection_result = PlainIntersection(input_data, comm_role, peer_comm_role, bucket_id, thread_num)
        else:
            intersection_type = "PSI"
            intersection_result = RunPSI(input_data, comm_role, peer_comm_role, bucket_id, thread_num)
        print("{} result: {} (display limit: 20), result size is: {}".format(
            intersection_type, intersection_result[:20], len(intersection_result)))
        if need_check:
            peer_input_data = generate_input_data(peer_input_begin, peer_input_end, peer_read_file, peer_file_name)
            actual_result = compute_right_result(input_data, peer_input_data)
            if check_psi(actual_result, intersection_result) == 0:
                print("success, {} check pass!".format(intersection_type))
            else:
                print("ERROR, {} check failed!".format(intersection_type))
