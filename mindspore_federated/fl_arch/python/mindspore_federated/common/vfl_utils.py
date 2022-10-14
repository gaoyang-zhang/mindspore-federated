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
"""Essential tools to modeling the split-learning process."""

import os.path
import struct
from typing import OrderedDict

import logging
import yaml

from mindspore import nn, ops, Tensor, ParameterTuple
from mindspore import dtype as mstype

from .vfl_pb2 import DataType, TensorListProto, TensorProto


def parse_yaml_file(file_path):
    yaml_path = os.path.abspath(file_path)
    if not os.path.exists(file_path):
        assert ValueError(f'File {yaml_path} not exit')
    with open(yaml_path, 'r', encoding='utf-8') as fp:
        yaml_data = yaml.safe_load(fp)
    return yaml_data, fp


class FLYamlData:
    """
    Data class storing configuration information on the vertical federated learning process, including
    inputs, outputs, and hyper-parameters of networks, optimizers, operators, etc. The information
    mentioned above is parsed from the yaml file provided by the developer of the vertical federated
    learning system. The class will verify the yaml file in the parsing process.

    Args:
        yaml_path: Path of the yaml file
    """
    def __init__(self, yaml_path: str):
        yaml_path = os.path.abspath(yaml_path)
        if not os.path.exists(yaml_path):
            raise ValueError(f'File {yaml_path} not exit')
        with open(yaml_path, 'r', encoding='utf-8') as self.fp:
            self.yaml_data = yaml.safe_load(self.fp)

        if 'role' not in self.yaml_data:
            raise ValueError('FLYamlData init failed: missing field of \'role\'')
        self.role = self.yaml_data['role']
        if self.role not in ['leader', 'follower', 'leader&follower']:
            raise ValueError(f'FLYamlData init failed: value of role ({self.role}) is illegal, \
                             role shall be either \'leader\' or \'follower\'')

        if 'model' not in self.yaml_data:
            raise ValueError('FLYamlData init failed: missing field of \'model\'')
        self.model_data = self.yaml_data['model']
        self._parse_train_net()
        self._parse_eval_net()

        if 'opts' not in self.yaml_data:
            raise ValueError('FLYamlData init failed: missing field of \'opts\'')
        self.opts = self.yaml_data['opts']
        self._check_opts()

        self.grad_scalers = self.yaml_data['grad_scalers'] if 'grad_scalers' in self.yaml_data else None
        if self.grad_scalers:
            self._check_grad_scalers()

        self.dataset = self.yaml_data['dataset'] if 'dataset' in self.yaml_data else None
        self.train_hyper_parameters = self.yaml_data['hyper_parameters'] \
            if 'hyper_parameters' in self.yaml_data else None

        if 'privacy' in self.yaml_data:
            self.privacy = self.yaml_data['privacy']
            if 'label_dp' not in self.privacy:
                raise ValueError('FLYamlData init failed: missing field of \'label_dp\' under \'privacy\'')
            if 'eps' not in self.privacy['label_dp']:
                raise ValueError('FLYamlData init failed: missing field of \'eps\' under \'label_dp\'')
            self.privacy_eps = self.privacy['label_dp']['eps']
        self.fp.close()

    def _parse_train_net(self):
        """Parse information on training net."""
        if 'train_net' not in self.model_data:
            raise ValueError('FLYamlData init failed: missing field of \'train_net\'')
        self.train_net = self.model_data['train_net']
        self.train_net_ins = [input_config for input_config in self.train_net['inputs']]
        if not self.train_net_ins:
            raise ValueError('FLYamlData init failed: inputs of \'train_net\' are empty')
        self.train_net_in_names = [input_config['name'] for input_config in self.train_net['inputs']]
        self.train_net_outs = [output_config for output_config in self.train_net['outputs']]
        if not self.train_net_outs:
            raise ValueError('FLYamlData init failed: outputs of \'train_net\' are empty')
        self.train_net_out_names = [input_config['name'] for input_config in self.train_net['outputs']]

    def _parse_eval_net(self):
        """Parse information on evaluation net."""
        if 'eval_net' not in self.model_data:
            raise ValueError('FLYamlData init failed: missing field of \'eval_net\'')
        self.eval_net = self.model_data['eval_net']
        self.eval_net_ins = [input_config for input_config in self.eval_net['inputs']]
        if not self.eval_net_ins:
            raise ValueError('FLYamlData init failed: inputs of \'eval_net\' are empty')
        self.eval_net_outs = [output_config for output_config in self.eval_net['outputs']]
        if not self.eval_net_outs:
            raise ValueError('FLYamlData init failed: outputs of \'eval_net\' are empty')
        self.eval_net_gt = self.eval_net['gt'] if 'gt' in self.eval_net else None

    def _check_opts(self):
        """Verify configurations of optimizers defined in the yaml file."""
        for opt_config in self.opts:
            for grad_config in opt_config['grads']:
                grad_inputs = {grad_in['name'] for grad_in in grad_config['inputs']}
                if not grad_inputs.issubset(self.train_net_in_names):
                    raise ValueError('optimizer %s config error: containing undefined inputs' % opt_config['type'])
                grad_out = grad_config['output']['name']
                if grad_out not in self.train_net_out_names:
                    raise ValueError('optimizer %s config error: containing undefined output %s'
                                     % (opt_config['type'], grad_out))
                if not isinstance('sens', (str, int, float)):
                    raise ValueError('optimizer %s config error: unsupported sens type of grads' % opt_config['type'])

    def _check_grad_scalers(self):
        """Verify configurations of grad_scales defined in the yaml file."""
        for grad_scale_config in self.grad_scalers:
            grad_scale_ins = {grad_scale_in['name'] for grad_scale_in in grad_scale_config['inputs']}
            if not grad_scale_ins.issubset(self.train_net_in_names):
                raise ValueError('grad_scales config error: containing undefined inputs')
            grad_scale_out = grad_scale_config['output']['name']
            if grad_scale_out not in self.train_net_out_names:
                raise ValueError('grad_scales config error: containing undefined output %s' % grad_scale_out)


def get_params_list_by_name(net, name):
    """
    Get parameters list by name from the nn.Cell

    Inputs:
        net (nn.Cell): Network described using mindspore.
        name (str): Name of parameters to be gotten.
    """
    res = []
    trainable_params = net.trainable_params()
    for param in trainable_params:
        if name in param.name:
            res.append(param)
    return res


def get_params_by_name(net, weight_name_list):
    """
    Get parameters list by names from the nn.Cell

    Inputs:
        net (nn.Cell): Network described using mindspore.
        name (list): Names of parameters to be gotten.
    """
    params = []
    for weight_name in weight_name_list:
        params.extend(get_params_list_by_name(net, weight_name))
    params = ParameterTuple(params)
    return params


class IthOutputCellInDict(nn.Cell):
    """
    Encapulate network with multiple outputs so that it only output one variable.

    Args:
        network (nn.Cell): Network to be encapulated.
        output_index (int): Index of the output variable.

    Inputs:
        **kwargs (dict): input of the network.
    """
    def __init__(self, network, output_index):
        super(IthOutputCellInDict, self).__init__()
        self.network = network
        self.output_index = output_index

    def construct(self, **kwargs):
        return self.network(**kwargs)[self.output_index]


class IthOutputCellInTuple(nn.Cell):
    """
    Encapulate network with multiple outputs so that it only output one variable.

    Args:
        network (nn.Cell): Network to be encapulated.
        output_index (int): Index of the output variable.

    Inputs:
        *kwargs (tuple): input of the network.
    """
    def __init__(self, network, output_index):
        super(IthOutputCellInTuple, self).__init__()
        self.network = network
        self.output_index = output_index

    def construct(self, *args):
        return self.network(*args)[self.output_index]


MSTYPE_DATATYPE_DICT = {
    mstype.float32: DataType.FLOAT,
    mstype.uint8: DataType.UINT8,
    mstype.int8: DataType.INT8,
    mstype.uint16: DataType.UINT16,
    mstype.int16: DataType.INT16,
    mstype.int32: DataType.INT32,
    mstype.int64: DataType.INT64,
    mstype.string: DataType.STRING,
    mstype.bool_: DataType.BOOL,
    mstype.float16: DataType.FLOAT16,
    mstype.double: DataType.DOUBLE,
    mstype.uint32: DataType.UINT32,
    mstype.uint64: DataType.UINT64,
    mstype.float64: DataType.FLOAT64
}

DATATYPE_MSTYPE_DICT = {
    DataType.FLOAT: mstype.float32,
    DataType.UINT8: mstype.uint8,
    DataType.INT8: mstype.int8,
    DataType.UINT16: mstype.uint16,
    DataType.INT16: mstype.int16,
    DataType.INT32: mstype.int32,
    DataType.INT64: mstype.int64,
    DataType.STRING: mstype.string,
    DataType.BOOL: mstype.bool_,
    DataType.FLOAT16: mstype.float16,
    DataType.DOUBLE: mstype.double,
    DataType.UINT32: mstype.uint32,
    DataType.UINT64: mstype.uint64,
    DataType.FLOAT64: mstype.float64
}

PROTO_DATA_MAP = {
    DataType.BOOL: 'int32_data',
    DataType.STRING: 'string_data',
    DataType.INT8: 'int32_data',
    DataType.INT16: 'int32_data',
    DataType.INT32: 'int32_data',
    DataType.INT64: 'int64_data',
    DataType.UINT8: 'uint32_data',
    DataType.UINT16: 'uint32_data',
    DataType.UINT32: 'uint32_data',
    DataType.UINT64: 'uint64_data',
    DataType.FLOAT: 'float_data',
    DataType.FLOAT16: 'float_data',
    DataType.DOUBLE: 'double_data',
    DataType.FLOAT64: 'double_data'
}


def get_proto_data_by_type(ts_proto: TensorProto, data_type):
    """
    Get the data type of a Tensor to construct a proto object.

    Inputs:
        ts_proto (class): the proto object of the tensor.
        data_type (enum): the data type of the tensor.
    """
    if not ts_proto:
        raise ValueError('get_proto_data_by_type: could not input a None ts_proto')
    if data_type not in PROTO_DATA_MAP:
        raise TypeError('get_proto_data_by_type: unsupported data_type ', data_type)
    data_attr_name = PROTO_DATA_MAP[data_type]
    return getattr(ts_proto, data_attr_name)


def tensor_to_proto(ts: Tensor, ref_key: str = None):
    """
    Convert a tensor to a proto.
    Inputs:
        ts (class): the Tensor object.
        ref_key (str): the ref name of the Tensor object.
    """
    if ts is None:
        raise ValueError('tensor_to_proto: could not input a Tensor with None value')
    ts_proto = TensorProto()
    if ref_key:
        ts_proto.ref_key = ref_key
    data_type = MSTYPE_DATATYPE_DICT.get(ts.dtype)
    if data_type is None:
        raise TypeError('tensor_to_proto: input a Tensor with unsupported value type ', ts.dtype)
    ts_proto.data_type = data_type
    ts_proto.dims.extend(ts.shape)
    ts_values = ts.reshape(ts.size,).asnumpy()
    get_proto_data_by_type(ts_proto, data_type).extend(ts_values)
    return ts_proto


def proto_to_tensor(ts_proto: TensorProto):
    """
    Convert a proto to a tensor.
    Inputs:
        ts_proto (class): the proto object of the Tensor.
    """
    ref_key = ts_proto.ref_key
    ts_type = DATATYPE_MSTYPE_DICT.get(ts_proto.data_type)
    if ts_type is None:
        logging.warning('proto_to_tensor: not specify data_type in TensorProto, using float32 by default')
        ts_type = mstype.float32
    values = get_proto_data_by_type(ts_proto, ts_proto.data_type)
    ts = Tensor(list(values), dtype=ts_type)
    ts = ops.reshape(ts, tuple(ts_proto.dims))
    return ref_key, ts


def tensor_dict_to_proto(ts_dict: dict, name: str = 'no_name', send_addr: str = 'localhost',
                         recv_addr: str = 'localhost'):
    """
    Convert a dict, the s of which are tensor objects, to proto object.
    Inputs:
        ts_dict (dict): the dict object, the items of which are tensors.
        name (str): the ref name of the proto object.
        send_addr (str): the send address of the proto object.
        recv_addr (str): the receive address of the proto object.
    """
    ts_list_proto = TensorListProto()
    ts_list_proto.name = name
    ts_list_proto.send_addr = send_addr
    ts_list_proto.recv_addr = recv_addr
    ts_list_proto.length = len(ts_dict)
    for ts_key, ts in ts_dict.items():
        if isinstance(ts, Tensor):
            ts_list_proto.tensors.append(tensor_to_proto(ts, ts_key))
        elif isinstance(ts, OrderedDict):
            ts_list_proto.tensor_list.append(tensor_dict_to_proto(ts, ts_key, send_addr, recv_addr))
    return ts_list_proto


def proto_to_tensor_dict(ts_list_proto: TensorListProto):
    """
    Parse a dict, the s of which are tensor objects, from a proto object.
    Inputs:
        ts_list_proto (class): the proto object.
    """
    name = ts_list_proto.name
    res = OrderedDict()
    tensors = list(ts_list_proto.tensors)
    for ts_proto in tensors:
        ref_key, ts = proto_to_tensor(ts_proto)
        res[ref_key] = ts
    tensor_dicts = list(ts_list_proto.tensor_list)
    for ts_list_proto_item in tensor_dicts:
        sub_name, sub_res = proto_to_tensor_dict(ts_list_proto_item)
        res[sub_name] = sub_res
    return name, res


def socket_read_nbytes(sock, n):
    """
    read n bytes from socket connection
    """
    buf = b''
    while n > 0:
        data = sock.recv(n)
        if not data:
            raise SystemError('unexpected socket connection close')
        buf += data
        n -= len(data)
    return buf


def send_proto(sock, proto_msg):
    """
    send a proto message through the socket connection.
    """
    msg = proto_msg.SerializeToString()
    msg_len = struct.pack('>L', len(msg))
    sock.sendall(msg_len + msg)
    return len(msg)


def recv_proto(sock):
    """
    receive a proto message from the socket connection.
    """
    msg_len = socket_read_nbytes(sock, 4)
    msg_len = struct.unpack('>L', msg_len)[0]
    msg_buf = socket_read_nbytes(sock, msg_len)
    return msg_buf, msg_len
