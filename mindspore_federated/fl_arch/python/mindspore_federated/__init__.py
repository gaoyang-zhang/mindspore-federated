# Copyright 2020 Huawei Technologies Co., Ltd
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
"""MindSpore Federated API, used to start server and scheduler, inits and uses worker"""

from .startup.federated_local import FLServerJob, FlSchedulerJob
from .startup.federated_local import Callback, CallbackContext
from .startup.feature_map import FeatureItem, FeatureMap
from .startup.ssl_config import SSLConfig
from .trainer._fl_manager import FederatedLearningManager, PushMetrics
from .common._fl_context import ServerModeHybrid, ServerModeFL
from . import log

__all__ = [
    "FLServerJob",
    "FlSchedulerJob",
    "Callback", "CallbackContext",
    "FeatureMap",
    "SSLConfig",
    "FederatedLearningManager",
    "PushMetrics",
    "ServerModeHybrid",
    "ServerModeFL"
]
