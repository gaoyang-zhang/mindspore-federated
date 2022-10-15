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
"""Export and load data in vfl."""

import os
from mindspore.mindrecord import FileWriter
from mindspore import dataset as ds


def export_mindrecord(file_name, raw_data, keys, shard_num=1, overwrite=True):
    """
    Export mindrecord.
    """
    writer = FileWriter(file_name=file_name,
                        shard_num=shard_num,
                        overwrite=overwrite)
    schema = raw_data.schema()
    desc = raw_data.desc()
    writer.add_schema(schema, desc)
    writer.open_and_set_header()
    for value in raw_data.values(keys=keys):
        writer.write_raw_data([value])
    writer.commit()


def load_mindrecord(input_dir, seed=0, **kwargs):
    ds.config.set_seed(seed)
    dataset_files = [os.path.join(input_dir, _) for _ in os.listdir(input_dir) if "db" not in _]
    dataset_files.sort()
    dataset = ds.MindDataset(dataset_files, **kwargs)
    return dataset
