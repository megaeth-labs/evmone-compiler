// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "caterpillar.hpp"
#include "execution_state.hpp"
#include "instructions.hpp"
#include "vm.hpp"
#include <evmc/instructions.h>
#include <memory>

namespace evmone::caterpillar
{
namespace
{
[[gnu::always_inline]] inline code_iterator invoke(void (*instr_fn)(StackTop), uint256* stack_top,
    code_iterator code_it, ExecutionState& /*state*/) noexcept
{
    instr_fn(stack_top);
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(StopToken (*instr_fn)(), uint256* /*stack_top*/,
    code_iterator /*code_it*/, ExecutionState& state) noexcept
{
    state.status = instr_fn().status;
    return nullptr;
}

[[gnu::always_inline]] inline code_iterator invoke(
    evmc_status_code (*instr_fn)(StackTop, ExecutionState&), uint256* stack_top,
    code_iterator code_it, ExecutionState& state) noexcept
{
    if (const auto status = instr_fn(stack_top, state); status != EVMC_SUCCESS)
    {
        state.status = status;
        return nullptr;
    }
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(void (*instr_fn)(StackTop, ExecutionState&),
    uint256* stack_top, code_iterator code_it, ExecutionState& state) noexcept
{
    instr_fn(stack_top, state);
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(
    code_iterator (*instr_fn)(StackTop, ExecutionState&, code_iterator), uint256* stack_top,
    code_iterator code_it, ExecutionState& state) noexcept
{
    return instr_fn(stack_top, state, code_it);
}

[[gnu::always_inline]] inline code_iterator invoke(StopToken (*instr_fn)(StackTop, ExecutionState&),
    uint256* stack_top, code_iterator /*code_it*/, ExecutionState& state) noexcept
{
    state.status = instr_fn(stack_top, state).status;
    return nullptr;
}
/// @}

template <evmc_opcode Op>
inline evmc_status_code check_defined(evmc_revision rev) noexcept
{
    static_assert(
        !(instr::has_const_gas_cost(Op) && instr::gas_costs[EVMC_FRONTIER][Op] == instr::undefined),
        "undefined instructions must not be handled by check_requirements()");

    if constexpr (!instr::has_const_gas_cost(Op))
    {
        if (INTX_UNLIKELY(instr::gas_costs[rev][Op] < 0))
            return EVMC_UNDEFINED_INSTRUCTION;
    }
    return EVMC_SUCCESS;
}

template <evmc_opcode Op>
inline evmc_status_code check_stack(ptrdiff_t stack_size) noexcept
{
    if constexpr (instr::traits[Op].stack_height_change > 0)
    {
        static_assert(instr::traits[Op].stack_height_change == 1);
        if (INTX_UNLIKELY(stack_size == Stack::limit))
            return EVMC_STACK_OVERFLOW;
    }
    if constexpr (instr::traits[Op].stack_height_required > 0)
    {
        if (INTX_UNLIKELY(stack_size < instr::traits[Op].stack_height_required))
            return EVMC_STACK_UNDERFLOW;
    }
    return EVMC_SUCCESS;
}

template <evmc_opcode Op>
inline evmc_status_code check_gas(int64_t& gas_left, evmc_revision rev) noexcept
{
    auto gas_cost = instr::gas_costs[EVMC_FRONTIER][Op];  // Init assuming const cost.
    if constexpr (!instr::has_const_gas_cost(Op))
        gas_cost = instr::gas_costs[rev][Op];  // If not, load the cost from the table.
    if (INTX_UNLIKELY((gas_left -= gas_cost) < 0))
        return EVMC_OUT_OF_GAS;

    return EVMC_SUCCESS;
}

template <evmc_opcode Op>
evmc_status_code invoke(const uint256* stack_bottom, uint256* stack_top, code_iterator code_it,
    void*, ExecutionState& state) noexcept;

evmc_status_code cat_undefined(const uint256* /*stack_bottom*/, uint256* /*stack_top*/,
    code_iterator /*code_it*/, void*, ExecutionState& /*state*/) noexcept
{
    return EVMC_UNDEFINED_INSTRUCTION;
}

using InstrFn = evmc_status_code (*)(const uint256* stack_bottom, uint256* stack_top,
    code_iterator code_it, void*, ExecutionState& state) noexcept;

constexpr auto instr_table = []() noexcept {
#define X(OPCODE, IDENTIFIER) invoke<OPCODE>,
#define X_UNDEFINED(OPCODE) cat_undefined,
    std::array<InstrFn, 256> table{MAP_OPCODE_TO_IDENTIFIER};
    return table;
#undef X
#undef X_UNDEFINED
}();
static_assert(std::size(instr_table) == 256);
static_assert(instr_table[OP_PUSH2] == invoke<OP_PUSH2>);

/// A helper to invoke the instruction implementation of the given opcode Op.
template <evmc_opcode Op>
evmc_status_code invoke(const uint256* stack_bottom, uint256* stack_top, code_iterator code_it,
    void* tbl, ExecutionState& state) noexcept
{
    [[maybe_unused]] auto op = Op;

    const auto rev = state.rev;
    if (const auto status = check_defined<Op>(rev); status != EVMC_SUCCESS)
        return status;

    const auto stack_size = stack_top - stack_bottom;
    if (const auto status = check_stack<Op>(stack_size); INTX_UNLIKELY(status != EVMC_SUCCESS))
        return status;

    if (const auto status = check_gas<Op>(state.gas_left, state.rev);
        INTX_UNLIKELY(status != EVMC_SUCCESS))
        return status;

    code_it = invoke(instr::core::impl<Op>, stack_top, code_it, state);
    if (!code_it)
        return state.status;

    stack_top += instr::traits[Op].stack_height_change;
    auto tbl2 = (InstrFn*)tbl;
    [[clang::musttail]] return tbl2[*code_it](stack_bottom, stack_top, code_it, tbl, state);
}

}  // namespace

evmc_result execute(
    const VM& /*vm*/, ExecutionState& state, const baseline::CodeAnalysis& analysis) noexcept
{
    state.analysis.baseline = &analysis;  // Assign code analysis for instruction implementations.

    // Use padded code.
    state.code = {analysis.padded_code.get(), state.code.size()};

    const auto code_it = state.code.data();
    const auto first_fn = instr_table[*code_it];
    const auto stack_bottom = state.stack.top_item;
    auto stack_top = stack_bottom;
    const auto status =
        first_fn(stack_bottom, stack_top, code_it, (void*)instr_table.data(), state);

    const auto gas_left = (status == EVMC_SUCCESS || status == EVMC_REVERT) ? state.gas_left : 0;
    const auto result = evmc::make_result(status, gas_left,
        state.output_size != 0 ? &state.memory[state.output_offset] : nullptr, state.output_size);
    return result;
}

evmc_result execute(evmc_vm* c_vm, const evmc_host_interface* host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept
{
    auto vm = static_cast<VM*>(c_vm);
    const auto jumpdest_map = baseline::analyze({code, code_size});
    auto state =
        std::make_unique<ExecutionState>(*msg, rev, *host, ctx, bytes_view{code, code_size});
    return caterpillar::execute(*vm, *state, jumpdest_map);
}

}  // namespace evmone::caterpillar
