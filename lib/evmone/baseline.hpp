// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.h>
#include <evmc/utils.h>
#include <vector>

namespace evmone
{
using JumpdestMap = std::vector<bool>;

EVMC_EXPORT JumpdestMap build_jumpdest_map(const uint8_t* code, size_t code_size);

evmc_result baseline_execute(evmc_vm* vm, const evmc_host_interface* host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept;
}  // namespace evmone
