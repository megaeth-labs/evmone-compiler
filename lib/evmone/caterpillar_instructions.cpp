// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "caterpillar_instructions.hpp"
#include "execution_state.hpp"
#include "instructions.hpp"
#include "vm.hpp"
#include <evmc/instructions.h>
#include <memory>

namespace evmone::caterpillar
{
[[gnu::always_inline]] inline code_iterator invoke(void (*instr_fn)(StackTop), uint256* stack_top,
    code_iterator code_it, int64_t /*gas*/, ExecutionState& /*state*/) noexcept
{
    instr_fn(stack_top);
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(StopToken (*instr_fn)(), uint256* /*stack_top*/,
    code_iterator /*code_it*/, int64_t /*gas*/, ExecutionState& state) noexcept
{
    state.status = instr_fn().status;
    return nullptr;
}

[[gnu::always_inline]] inline code_iterator invoke(
    evmc_status_code (*instr_fn)(StackTop, ExecutionState&), uint256* stack_top,
    code_iterator code_it, int64_t /*gas*/, ExecutionState& state) noexcept
{
    if (const auto status = instr_fn(stack_top, state); status != EVMC_SUCCESS)
    {
        state.status = status;
        return nullptr;
    }
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(void (*instr_fn)(StackTop, ExecutionState&),
    uint256* stack_top, code_iterator code_it, int64_t /*gas*/, ExecutionState& state) noexcept
{
    instr_fn(stack_top, state);
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(
    code_iterator (*instr_fn)(StackTop, ExecutionState&, code_iterator), uint256* stack_top,
    code_iterator code_it, int64_t /*gas*/, ExecutionState& state) noexcept
{
    return instr_fn(stack_top, state, code_it);
}

[[gnu::always_inline]] inline code_iterator invoke(StopToken (*instr_fn)(StackTop, ExecutionState&),
    uint256* stack_top, code_iterator /*code_it*/, int64_t /*gas*/, ExecutionState& state) noexcept
{
    state.status = instr_fn(stack_top, state).status;
    return nullptr;
}
/// @}

template <evmc_opcode Op>
inline bool check_stack(ptrdiff_t stack_size) noexcept
{
    if constexpr (instr::traits[Op].stack_height_change > 0)
    {
        static_assert(instr::traits[Op].stack_height_change == 1);
        if (stack_size == Stack::limit)
            return false;
    }
    if constexpr (instr::traits[Op].stack_height_required > 0)
    {
        if (stack_size < instr::traits[Op].stack_height_required)
            return false;
    }
    return true;
}

inline constexpr bool has_const_gas_cost_since_defined(evmc_opcode op) noexcept
{
    const size_t first_rev = *instr::traits[op].since;
    const auto g = instr::gas_costs[first_rev][op];
    for (size_t r = first_rev + 1; r <= EVMC_MAX_REVISION; ++r)
    {
        if (instr::gas_costs[r][op] != g)
            return false;
    }
    return true;
}
static_assert(has_const_gas_cost_since_defined(OP_STOP));
static_assert(has_const_gas_cost_since_defined(OP_ADD));
static_assert(has_const_gas_cost_since_defined(OP_PUSH1));
static_assert(has_const_gas_cost_since_defined(OP_SHL));
static_assert(!has_const_gas_cost_since_defined(OP_BALANCE));
static_assert(!has_const_gas_cost_since_defined(OP_SLOAD));

template <evmc_opcode Op>
inline int64_t check_gas(int64_t gas_left, evmc_revision rev) noexcept
{
    auto gas_cost = instr::gas_costs[*instr::traits[Op].since][Op];  // Init assuming const cost.
    if constexpr (!has_const_gas_cost_since_defined(Op))
        gas_cost = instr::gas_costs[rev][Op];  // If not, load the cost from the table.
    return gas_left - gas_cost;
}

template <evmc_opcode Op>
evmc_status_code invoke(
    uint256* stack_top, code_iterator code_it, int64_t /*gas*/, ExecutionState& state) noexcept;

evmc_status_code cat_undefined(uint256* /*stack_top*/, code_iterator /*code_it*/, int64_t /*gas*/,
    ExecutionState& /*state*/) noexcept
{
    return EVMC_UNDEFINED_INSTRUCTION;
}

#define X_UNDEFINED(OPCODE)
#define X(OPCODE, IDENTIFIER)                                                                 \
    evmc_status_code OPCODE##_(                                                               \
        uint256* stack_top, code_iterator code_it, int64_t g, ExecutionState& state) noexcept \
    {                                                                                         \
        return invoke<OPCODE>(stack_top, code_it, g, state);                                  \
    }

MAP_OPCODE_TO_IDENTIFIER
#undef X
#undef X_UNDEFINED

/// A helper to invoke the instruction implementation of the given opcode Op.
template <evmc_opcode Op>
evmc_status_code invoke(
    uint256* stack_top, code_iterator code_it, int64_t gas, ExecutionState& state) noexcept
{
    [[maybe_unused]] auto op = Op;

    const auto stack_size = stack_top - state.stack_bottom;
    if (INTX_UNLIKELY(!check_stack<Op>(stack_size)))
        return EVMC_FAILURE;

    if (gas = check_gas<Op>(gas, state.rev); INTX_UNLIKELY(gas < 0))
        return EVMC_FAILURE;

    state.gas_left = gas;
    code_it = invoke(instr::core::impl<Op>, stack_top, code_it, gas, state);
    gas = state.gas_left;
    if (!code_it)
        return state.status;

    stack_top += instr::traits[Op].stack_height_change;
    const auto tbl = static_cast<const InstrFn*>(state.tbl);
    [[clang::musttail]] return tbl[*code_it](stack_top, code_it, gas, state);
}

}  // namespace evmone::caterpillar
