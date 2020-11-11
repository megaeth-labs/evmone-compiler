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
    static constexpr auto bpw = 8;
    using word_type = uint8_t;
    std::unique_ptr<word_type[]> words_;
    std::size_t size_;

public:
    explicit bitset(std::size_t size)
      : words_{new word_type[(size + (bpw - 1)) / bpw]}, size_{size}
    {
        std::memset(words_.get(), 0, (size + (bpw - 1)) / bpw);
    }

    std::size_t size() const noexcept { return size_; }

    void set(std::size_t index) noexcept
    {
        const auto w = index / bpw;
        const auto x = index % bpw;
        const auto bitmask = word_type(word_type{1} << x);
        words_[w] |= bitmask;
    }

    bool operator[](std::size_t index) const noexcept
    {
        const auto w = index / bpw;
        const auto x = index % bpw;
        const auto bitmask = word_type{1} << x;
        return (words_[w] & bitmask) != 0;
    }
};

using JumpdestMap = bitset;

EVMC_EXPORT JumpdestMap build_jumpdest_map(const uint8_t* code, size_t code_size);

evmc_result baseline_execute(evmc_vm* vm, const evmc_host_interface* host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept;
}  // namespace evmone
