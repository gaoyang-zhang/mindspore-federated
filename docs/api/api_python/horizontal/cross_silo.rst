云云联邦学习
================================

.. py:class:: mindspore_federated.FederatedLearningManager(yaml_config, model, sync_frequency, http_server_address="", data_size=1, sync_type='fixed', ssl_config=None, **kwargs)

    在训练过程中管理联邦学习。

    参数：
        - **yaml_config** (str) - yaml文件路径。更多细节见 `yaml配置说明 <https://gitee.com/mindspore/federated/blob/master/docs/api/api_python/horizontal/federated_server_yaml.md>`_。
        - **model** (nn.Cell) - 一个用于联邦训练的模型。
        - **sync_frequency** (int) - 联邦学习中的参数同步频率。
          需要注意在数据下沉模式中，频率的值等于epoch。否则，频率的值等于step。
          在自适应同步频率模式下为初始同步频率，在固定频率模式下为同步频率。
        - **http_server_address** (str) - 用于通信的http服务器地址。默认值：“”。
        - **data_size** (int) - 需要向worker报告的数据量。默认值：1。
        - **sync_type** (str) - 采用同步策略类型的参数。
          支持["fixed", "adaptive"]。默认值："fixed"。
          - fixed：参数的同步频率是固定的。
          - adaptive：参数的同步频率是自适应变化的。
        - **sl_config** (Union(None, SSLConfig)) - ssl配置项。默认值：None。
        - **min_consistent_rate** (float) - 最小一致性比率阈值，该值越大同步频率提升难度越大。
          取值范围：大于等于0.0。默认值：1.1。
        - **min_consistent_rate_at_round** (int) - 最小一致性比率阈值的轮数，该值越大同步频率提升难度越大。
          取值范围：大于等于0。默认值：0。
        - **ema_alpha** (float) - 梯度一致性平滑系数，该值越小越会根据当前轮次的梯度分叉情况来判断频率是否
          需要改变，反之则会更加根据历史梯度分叉情况来判断。
          取值范围：(0.0, 1.0)。默认值：0.5。
        - **observation_window_size** (int) - 观察时间窗的轮数，该值越大同步频率减小难度越大。
          取值范围：大于0。默认值：5。
        - **frequency_increase_ratio** (int) - 频率提升幅度，该值越大频率提升幅度越大。
          取值范围：大于0。默认值：2。
        - **unchanged_round** (int) - 频率不发生变化的轮数，在前unchanged_round个轮次，频率不会发生变化。
          取值范围：大于等于0。默认值：0。

    .. note::
        这是一个实验原型，可能会有变化。

    .. py:method:: start_pull_weight()

        在混合训练模式中，从服务器拉取权重。

    .. py:method:: start_push_weight()

        在混合训练模式中，向服务器推送权重。

    .. py:method:: step_end(run_context)

        在step结束时同步参数。如果 `sync_type` 是"adaptive"，同步频率会在这里自适应的调整。

        参数：
            - **run_context** (RunContext) - 包含模型的相关信息。