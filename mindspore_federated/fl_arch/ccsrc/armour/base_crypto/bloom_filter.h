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

#ifndef MINDSPORE_FEDERATED_BLOOM_FILTER_H
#define MINDSPORE_FEDERATED_BLOOM_FILTER_H

#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>

#include "common/parallel_for.h"

namespace mindspore {
namespace fl {
namespace psi {
const unsigned char numToBin[8] = {
  0b10000000, 0b01000000, 0b00100000, 0b00010000, 0b00001000, 0b00000100, 0b00000010, 0b00000001,
};

constexpr size_t BYTE_LENGTH = 8;

struct BloomFilter {
  // negLogfpRate = -log(fpRate), default fpRate is 2^-40
  explicit BloomFilter(const std::vector<std::string> &intup_vct, int neg_log_fp_ate = 40)
      : BloomFilter("", intup_vct.size(), neg_log_fp_ate) {
    time_t time_start;
    time_t time_end;
    time(&time_start);

    std::vector<unsigned char> long_array(array_bit_length_ + BYTE_LENGTH - 1, 0);
    ParallelSync parallel_sync(0);
    parallel_sync.parallel_for(0, input_num_, 1, [&](size_t beg, size_t end) {
      for (size_t i = beg; i < end; i++) {
        for (size_t j = 0; j < hash_num_; j++) {
          size_t hashResult = ezHash(intup_vct[i], j);
          long_array[hashResult] = 1;
        }
      }
    });
    time(&time_end);
    MS_LOG(INFO) << "Hash and insert time cost: " << difftime(time_end, time_start) << " s.";

    time(&time_start);
    // compress
    parallel_sync.parallel_for(0, long_array.size() / BYTE_LENGTH, 1, [&](size_t beg, size_t end) {
      for (size_t i = beg; i < end; i++) {
        unsigned char tmp = 0;
        for (size_t j = 0; j < BYTE_LENGTH; j++) {
          tmp <<= 1;
          tmp += long_array[i * BYTE_LENGTH + j];
        }
        bit_array_[i] = tmp;
      }
    });

    time(&time_end);
    MS_LOG(INFO) << "Generate filter time cost: " << difftime(time_end, time_start) << " s.";
    MS_LOG(INFO) << "Bloom filter bit array size is: " << bitArrayByteLen() << "bytes.";
  }

  BloomFilter(const std::string &bit_array, size_t input_num, int neg_log_fp_rate = 40) {
    input_num_ = input_num;
    hash_num_ = neg_log_fp_rate;
    bits_of_per_item_ = (size_t)(neg_log_fp_rate / M_LN2) + 1;
    array_bit_length_ = input_num * bits_of_per_item_;
    hash_out_of_bits_ = (size_t)log2(static_cast<double>(array_bit_length_));
    bit_array_ = new unsigned char[bitArrayByteLen()];
    if (!bit_array.empty())
      bit_array_ = reinterpret_cast<uint8_t *>(memcpy(bit_array_, bit_array.c_str(), bit_array.size()));
  }

  size_t ezHash(const std::string &inputStr, size_t startPos) const {
    size_t ret = 0;

    size_t i = 0;
    while (BYTE_LENGTH * (i + 1) <= hash_out_of_bits_) {
      ret <<= BYTE_LENGTH;
      ret += (unsigned char)inputStr[(startPos + i) % inputStr.size()];
      i++;
    }
    size_t tmp = (unsigned char)inputStr[(startPos + i) % inputStr.size()];
    size_t mov = hash_out_of_bits_ - BYTE_LENGTH * i + startPos / inputStr.size();
    ret <<= mov;
    ret += (tmp >> (BYTE_LENGTH - mov));
    ret %= array_bit_length_;
    return ret;
  }

  bool LookUp(const std::string &lookupStr) const {
    bool ret = true;
    for (size_t i = 0; i < hash_num_; i++) {
      size_t hashResult = ezHash(lookupStr, i);
      size_t index = hashResult / BYTE_LENGTH;
      ret &= static_cast<bool>(bit_array_[index] & numToBin[hashResult % BYTE_LENGTH]);
    }
    return ret;
  }

  size_t bitArrayByteLen() const { return (array_bit_length_ + BYTE_LENGTH - 1) / BYTE_LENGTH; }

  std::string GetData() const {
    std::string ret;
    ret.reserve(bitArrayByteLen());
    for (size_t i = 0; i < bitArrayByteLen(); i++) {
      ret += static_cast<char>(bit_array_[i]);
    }
    return ret;
  }

  size_t input_num_;
  uint8_t *bit_array_;
  size_t array_bit_length_;
  size_t bits_of_per_item_;
  size_t hash_num_ = 40;
  size_t hash_out_of_bits_;
};

}  // namespace psi
}  // namespace fl
}  // namespace mindspore

#endif  // MINDSPORE_FEDERATED_BLOOM_FILTER_H
