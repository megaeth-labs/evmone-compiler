// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "instructions.hpp"
#include "instructions_xmacro.hpp"

namespace evmone
{
class VM;

namespace caterpillar
{
using InstrFn = evmc_status_code (*)(
    uint256* stack_top, code_iterator code_it, int64_t, ExecutionState& state) noexcept;

evmc_status_code cat_undefined(uint256* /*stack_top*/, code_iterator /*code_it*/, int64_t /*gas*/,
    ExecutionState& /*state*/) noexcept;

#define X_UNDEFINED(OPCODE)
#define X(OPCODE, IDENTIFIER)   \
    evmc_status_code OPCODE##_( \
        uint256* stack_top, code_iterator code_it, int64_t g, ExecutionState& state) noexcept;

MAP_OPCODE_TO_IDENTIFIER
#undef X
#undef X_UNDEFINED


}  // namespace caterpillar
}  // namespace evmone
