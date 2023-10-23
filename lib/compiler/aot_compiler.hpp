// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "aot_instructions.hpp"
#include "aot_baseline_instruction_table.hpp"

/// Optimization 1: merge consecutive PUSH and JUMP(I) instructions
#ifndef ENABLE_PUSHnJUMP
#define ENABLE_PUSHnJUMP 1
#endif

/// Optimization 2: choose where to insert the out-of-gas check (this helps the
/// compiler to generate better code sometimes).
/// Possible values:
/// - 0: BLOCK_START
/// - 1: Before JUMP (appears to be the best option in most cases)
/// - 2: JUMPDEST
///
/// The most natural place to perform the gas check is at the beginning of each
/// basic block. However, we may also check it before JUMP(I)'s or JUMPDEST's.
/// Programs are still guaranteed to terminate since every loop iteration must
/// execute the jump once. However, a malicious program may create a huge basic
/// block to incur significantly more computation than its fee allows. We can
/// easily prevent this attack by selectively apply this optimization only if a
/// basic block is not ridiculously long.
#ifndef GAS_CHECK_LOC
#define GAS_CHECK_LOC 1
#endif

/// "Optimization" 3: turn off the gas check entirely (but leave gas metering).
/// This is not a plausible optimization on its own because a program may now
/// run forever. For now, this is only used in performance experiments.
#ifndef GAS_CHECK_OFF
#define GAS_CHECK_OFF 0
#endif

/// Mark the beginning of a basic block.
///
/// The important trick here is to move "status = EVMC_OUT_OF_GAS" when gas < 0 into
/// the EPILOGUE (see below). This allows the compiler to generate much better code
/// (TODO: I am not yet entirely sure why).
#define BLOCK_START(ofs, ...) \
  L_OFFSET_##ofs:                                                                   \
    static constexpr BasicBlock bb_##ofs {__VA_ARGS__};                             \
    status = check_block_requirements(bb_##ofs, gas, &stack.top(), stack_bottom);   \
    if ((!GAS_CHECK_OFF && (GAS_CHECK_LOC == 0) && (gas < 0)) ||                    \
            status != EVMC_SUCCESS) [[unlikely]]                                    \
        goto label_final;

#define INVOKE(Op, ...)                                                                                 \
    instr::core::impl<OP_##Op>(stack, gas, status, jump_addr, state __VA_OPT__(,) __VA_ARGS__);         \
    if constexpr (OP_##Op == OP_JUMPDEST) {                                                             \
        if (!GAS_CHECK_OFF && (GAS_CHECK_LOC == 2) && (gas < 0))                                        \
            goto label_final;                                                                           \
    }                                                                                                   \
    if constexpr (evmone::instr::has_extra_error_cases(OP_##Op)) {                                      \
        if (status != EVMC_SUCCESS) [[unlikely]]                                                        \
            goto label_final;                                                                           \
    }                                                                                                   \
    if constexpr (OP_##Op == OP_STOP || OP_##Op == OP_RETURN)                                           \
        goto label_final;                                                                               \
    if constexpr (OP_##Op == OP_JUMP)                                                                   \
        goto *jump_addr;                                                                                \
    else if constexpr (OP_##Op == OP_JUMPI) {                                                           \
        if (jump_addr) goto *jump_addr;                                                                 \
    }

/// Implement the super-instruction "PUSHnJUMP". Note that program labels must be
/// written out as literals! As of 10/2023, clang++-17 can't seem to generate optimal
/// code even if the goto location is a constexpr.
#if ENABLE_PUSHnJUMP
#define PUSHnJUMP(ofs)                                                      \
    if ((GAS_CHECK_OFF || (GAS_CHECK_LOC != 1) || (gas >= 0)) &&            \
            jumpdest_map.is_jumpdest(ofs)) [[likely]]                       \
        goto L_OFFSET_##ofs;                                                \
    else                                                                    \
        goto label_final;
#else
#define PUSHnJUMP(ofs)                                                      \
    INVOKE(PUSH32, ofs)                                                     \
    INVOKE(JUMP, jumpdest_map)
#endif

#if ENABLE_PUSHnJUMP
#define PUSHnJUMPI(ofs)                                                     \
    if (stack.pop()) {                                                      \
        if ((GAS_CHECK_OFF || (GAS_CHECK_LOC != 1) || (gas >= 0)) &&        \
                jumpdest_map.is_jumpdest(ofs)) [[likely]]                   \
            goto L_OFFSET_##ofs;                                            \
        else                                                                \
            goto label_final;                                               \
    }
#else
#define PUSHnJUMPI(ofs)                                                     \
    INVOKE(PUSH32, ofs)                                                     \
    INVOKE(JUMPI, jumpdest_map)
#endif

#define PROLOGUE                                                    \
    _Pragma("GCC diagnostic push")                                  \
    _Pragma("GCC diagnostic ignored \"-Wunused-label\"")            \
    _Pragma("GCC diagnostic ignored \"-Wgnu-label-as-value\"")      \
    using namespace evmone::intx;                                   \
    int64_t gas = state.msg->gas;                                   \
    StackTop stack(state.stack_space.bottom());                     \
    auto stack_bottom = &stack.top();                               \
    evmc_status_code status = EVMC_SUCCESS;                         \
    native_jumpdest jump_addr {};                                   \
    state.bad_jump_handler = &&label_final;

#define EPILOGUE                        \
  label_final:                          \
    if (gas < 0)                        \
        status = EVMC_OUT_OF_GAS;       \
    state.status = status;              \
    return make_result(gas, state);     \
    _Pragma("GCC diagnostic pop")


namespace evmone
{

/// Summary of a basic block.
struct BasicBlock
{
    /// Base gas costs of this basic block.
    int64_t base_gas_cost {};

    /// Minimum stack height required by this basic block (to avoid stack underflow).
    int stack_required {};

    /// Growth in stack height after executing this basic block (may be negative).
    int stack_max_growth {};
};

/// Check if the current VM state satisfies the minimum requirements of executing a basic block.
inline evmc_status_code
check_block_requirements(const BasicBlock& basic_block, int64_t& gas_left,
    const uint256* stack_top, const uint256* stack_bottom) noexcept
{
    gas_left -= basic_block.base_gas_cost;
    // Eliding the gas check seems to produce 10x faster code for fib.
#if 0
    if (gas_left < 0) [[unlikely]]
        return EVMC_OUT_OF_GAS;
#endif
    if (stack_top < stack_bottom + basic_block.stack_required) [[unlikely]]
        return EVMC_STACK_UNDERFLOW;
    if (stack_top + basic_block.stack_max_growth > stack_bottom + 1024) [[unlikely]]
        return EVMC_STACK_OVERFLOW;
    return EVMC_SUCCESS;
}

/// Prepare the execution result of an EVM transaction.
inline evmc_result
make_result(int64_t gas, ExecutionState& state)
{
    const auto gas_left = (state.status == EVMC_SUCCESS || state.status == EVMC_REVERT) ? gas : 0;
    const auto gas_refund = (state.status == EVMC_SUCCESS) ? state.gas_refund : 0;

    assert(state.output_size != 0 || state.output_offset == 0);
    const auto result = evmc::make_result(state.status, gas_left, gas_refund,
        state.output_size != 0 ? &state.memory[state.output_offset] : nullptr, state.output_size);
    return result;
}

} // namespace evmone
