/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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

package com.mindspore.flclient.compression;

import com.mindspore.flclient.LocalFLParameter;
import static mindspore.fl.schema.CompressType.DIFF_SPARSE_QUANT;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.PriorityQueue;

/**
 * Encode Executor
 *
 * @since 2021-12-21
 */
public class EncodeExecutor {
    private final LocalFLParameter localFLParameter = LocalFLParameter.getInstance();

    private static volatile EncodeExecutor encodeExecutor;

    private EncodeExecutor() {}

    public static EncodeExecutor getInstance() {
        if (encodeExecutor == null) {
            synchronized (EncodeExecutor.class) {
                if (encodeExecutor == null) {
                    encodeExecutor = new EncodeExecutor();
                }
            }
        }
        return encodeExecutor;
    }

    private static final int multiplier = 2147483647;
    private static final double increment = 4294967294.0;
    private static final int modulo = 48271;

    public boolean[] constructMaskArray(int paramNum) {
        int seed = localFLParameter.getSeed();
        float uploadSparseRatio = localFLParameter.getUploadSparseRatio();

        boolean[] maskArray = new boolean[paramNum];

        int retain_num = (int) ((float) (paramNum) * uploadSparseRatio);
        for (int i = 0; i < retain_num; ++i) {
            maskArray[i] = true;
        }
        for (int i = retain_num; i < paramNum; ++i) {
            maskArray[i] = false;
        }

        seed = ((seed + multiplier) * modulo) % multiplier;
        for (int i = 0; i < paramNum; ++i) {
            // generate random number in (0, 1)
            double rand = (double) (seed) / increment + 0.5;
            // update seed
            seed = (seed * modulo) % multiplier;

            int j = (int) (rand * (double) (paramNum - i)) + i;
            boolean temp = maskArray[i];
            maskArray[i] = maskArray[j];
            maskArray[j] = temp;
        }
        return maskArray;
    }

    static public CompressWeight enDiffSparseQuantData(String featureName, float[] feature, float[] dataBeforeTrain,
                                                      int numBits, int trainDataSize, boolean[] maskArray, int maskPos) {
        if (feature.length != dataBeforeTrain.length) {
            throw new RuntimeException("The featurn len is not same after train, before train length is:" +
                    dataBeforeTrain.length + " after trian length is :" + feature.length);
        }
        float[] diffs = new float[dataBeforeTrain.length];
        int length = dataBeforeTrain.length;
        for (int i = 0; i < length; ++i) {
            diffs[i] = feature[i] - dataBeforeTrain[i] * (float) trainDataSize;
        }
        float[] sparseFeatureMap = new float[dataBeforeTrain.length];
        int sparseSize = 0;
        for (float dataValue : diffs) {
            if (maskArray[maskPos]) {
                sparseFeatureMap[sparseSize] = dataValue;
                sparseSize++;
            }
            maskPos += 1;
        }

        // quant encode
        float temp1 = (float) (1 << numBits) - 1.0f;
        float temp2 = (float) (1 << (numBits - 1));
        CompressWeight compressWeight = new CompressWeight();
        compressWeight.setWeightFullname(featureName);

        // get min and max value
        float minVal = Float.MAX_VALUE;
        float maxVal = -minVal;
        for (int i = 0; i < sparseSize; i++) {
            if (sparseFeatureMap[i] < minVal) {
                minVal = sparseFeatureMap[i];
            }
            if (sparseFeatureMap[i] > maxVal) {
                maxVal = sparseFeatureMap[i];
            }
        }
        compressWeight.setMinValue(minVal);
        compressWeight.setMaxValue(maxVal);
        float scale_value = (maxVal - minVal) / temp1 + 1e-10f;
        byte[] compressData = new byte[sparseSize];
        for (int i = 0; i < sparseSize; i++) {
            compressData[i] = (byte) (Math.round((sparseFeatureMap[i] - minVal) / scale_value - temp2));
        }
        compressWeight.setCompressData(compressData);
        return compressWeight;
    }
}