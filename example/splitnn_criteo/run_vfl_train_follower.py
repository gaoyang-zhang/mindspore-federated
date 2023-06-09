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
"""
Execute Wide&Deep split nn demo follower training on Criteo dataset with type of MindRecord.
The embeddings and grad scales are transmitted through http.
"""

import logging

from mindspore import set_seed
from mindspore import context
from mindspore_federated import FLModel, FLYamlData
from mindspore_federated.startup.vertical_federated_local import VerticalFederatedCommunicator, ServerConfig

from mindspore_federated.startup.ssl_config import SSLConfig
from wide_and_deep import WideDeepModel, BottomLossNet

from network_config import config
from run_vfl_train_local import construct_local_dataset

set_seed(0)


class FollowerTrainer:
    """Process of follower party"""

    def __init__(self):
        super(FollowerTrainer, self).__init__()
        logging.info('start vfl trainer success')

        if config.simu_tee:
            follower_bottom_yaml_data = FLYamlData(config.follower_bottom_tee_yaml_path)
        else:
            follower_bottom_yaml_data = FLYamlData(config.follower_bottom_yaml_path)

        follower_bottom_eval_net = follower_base_net = WideDeepModel(config, config.follower_field_size)
        follower_bottom_train_net = BottomLossNet(follower_base_net, config)
        self.follower_bottom_fl_model = FLModel(yaml_data=follower_bottom_yaml_data,
                                                network=follower_bottom_train_net,
                                                eval_network=follower_bottom_eval_net)

        # get compress config
        compress_configs = self.follower_bottom_fl_model.get_compress_configs()

        # build vertical communicator
        enable_ssl = config.enable_ssl
        http_server_config = ServerConfig(server_name='follower', server_address=config.http_server_address)
        remote_server_config = ServerConfig(server_name='leader', server_address=config.remote_server_address)
        if enable_ssl:
            ssl_config = SSLConfig(server_password=config.server_password, client_password=config.client_password,
                                   server_cert_path=config.server_cert_path,
                                   client_cert_path=config.client_cert_path,
                                   ca_cert_path=config.ca_cert_path)

            self.vertical_communicator = VerticalFederatedCommunicator(http_server_config=http_server_config,
                                                                       remote_server_config=remote_server_config,
                                                                       enable_ssl=True,
                                                                       ssl_config=ssl_config,
                                                                       compress_configs=compress_configs)
        else:
            self.vertical_communicator = VerticalFederatedCommunicator(http_server_config=http_server_config,
                                                                       remote_server_config=remote_server_config,
                                                                       compress_configs=compress_configs)
        self.vertical_communicator.launch()
        logging.info('Init follower trainer finish.')

    def start(self):
        """
        Run follower trainer
        """
        logging.info('Begin follower trainer')
        if config.resume:
            self.follower_bottom_fl_model.load_ckpt()
        for _ in range(config.epochs):
            for _, item in enumerate(train_iter):
                follower_embedding = self.follower_bottom_fl_model.forward_one_step(item)
                self.vertical_communicator.send_tensors("leader", follower_embedding)
                scale = self.vertical_communicator.receive("leader")
                self.follower_bottom_fl_model.backward_one_step(item, sens=scale)
            self.follower_bottom_fl_model.save_ckpt()
            for eval_item in eval_iter:
                follower_embedding = self.follower_bottom_fl_model.forward_one_step(eval_item)
                self.vertical_communicator.send_tensors("leader", follower_embedding)


logging.basicConfig(filename='follower_train.log', level=logging.INFO,
                    format='%(asctime)s %(levelname)s: %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
context.set_context(mode=context.GRAPH_MODE, device_target=config.device_target)
ds_train, ds_eval = construct_local_dataset()
train_iter = ds_train.create_dict_iterator()
eval_iter = ds_eval.create_dict_iterator()
train_size = ds_train.get_dataset_size()
eval_size = ds_eval.get_dataset_size()
logging.info("train_size is: %d", train_size)
logging.info("eval_size is: %d", eval_size)

if __name__ == '__main__':
    follower_trainer = FollowerTrainer()
    follower_trainer.start()
