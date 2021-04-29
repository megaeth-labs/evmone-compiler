// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.h>
#include <evmc/utils.h>
#include <cstring>
#include <memory>
#include <vector>

namespace evmone
{
class bitset
{
    using word_type = uint32_t;
    static constexpr auto word_bit = 8 * sizeof(word_type);
    std::unique_ptr<word_type[]> words_;
    std::size_t size_;

    static constexpr std::size_t get_num_words_required(std::size_t size) noexcept
    {
        return (size + (word_bit - 1)) / word_bit;
    }

public:
    explicit bitset(std::size_t size)
      : words_{new word_type[get_num_words_required(size)]}, size_{size}
    {
        std::memset(words_.get(), 0, get_num_words_required(size) * sizeof(word_type));
    }

    std::size_t size() const noexcept { return size_; }

    void set(std::size_t index) noexcept
    {
        auto& word = words_[index / word_bit];
        const auto bitmask = word_type{1} << (index % word_bit);
        word |= bitmask;
    }

    void unset(std::size_t index) noexcept
    {
        auto& word = words_[index / word_bit];
        const auto bitmask = word_type{1} << (index % word_bit);
        word &= ~bitmask;
    }

    bool operator[](std::size_t index) const noexcept
    {
        const auto& word = words_[index / word_bit];
        const auto bitmask = word_type{1} << (index % word_bit);
        return (word & bitmask) != 0;
    }

    void set_size(std::size_t size) noexcept { size_ = size; }
};

using JumpdestMap = bitset;

EVMC_EXPORT JumpdestMap build_jumpdest_map(const uint8_t* code, size_t code_size);

evmc_result baseline_execute(evmc_vm* vm, const evmc_host_interface* host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept;
}  // namespace evmone
