/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
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

package com.mindspore.flclient.model;

import com.mindspore.Model;
import com.mindspore.flclient.Common;
import com.mindspore.flclient.common.FLLoggerGenerater;
import com.mindspore.MSTensor;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.logging.Logger;

/**
 * Defining the Callback base class.
 *
 * @since v1.0
 */
public abstract class Callback {
    private static final Logger logger = FLLoggerGenerater.getModelLogger(LossCallback.class.toString());

    protected Model model;

    public int steps = 0;

    int epochs = 0;

    /**
     * Defining a constructor of  Callback.
     */
    public Callback(Model model) {
        this.model = model;
    }

    /**
     * searchOutputsForSize MSTensor need call MSTensor.free to recycle memory
     * this function is Deprecated, now you can use getOutputsBySize instead.
     *
     * @param size
     * @return
     */
    @Deprecated
    protected Optional<MSTensor> searchOutputsForSize(int size) {
        if (model == null) {
            logger.severe("trainSession cannot be null");
            return Optional.empty();
        }
        List<MSTensor> outputs = model.getOutputs();
        for (MSTensor tensor : outputs) {
            if (tensor == null) {
                logger.severe("tensor cannot be null");
                return Optional.empty();
            }
            if (tensor.elementsNum() == size) {
                return Optional.of(tensor);
            }
        }
        logger.severe("can not find output the tensor,element num is " + size);
        return Optional.empty();
    }

    protected Map<String, float[]> getOutputsBySize(int size) {
        if (model == null) {
            throw new RuntimeException("model cannot be null");
        }
        HashMap<String, float[]> mapOutputs = new HashMap<>();
        List<MSTensor> outputs = model.getOutputs();
        for (MSTensor tensor : outputs) {
            if (tensor == null) {
                logger.severe("tensor cannot be null");
            }
            if (tensor.elementsNum() == size) {
                mapOutputs.put(tensor.tensorName(), tensor.getFloatData());
            }
            tensor.free();
        }
        return mapOutputs;
    }

    /**
     * Step begin execute function.
     *
     * @return execute status.
     */
    public abstract Status stepBegin();

    /**
     * Step end execute function.
     *
     * @return execute status.
     */
    public abstract Status stepEnd();

    /**
     * epoch begin execute function.
     *
     * @return execute status.
     */
    public abstract Status epochBegin();

    /**
     * epoch end execute function.
     *
     * @return execute status.
     */
    public abstract Status epochEnd();
}