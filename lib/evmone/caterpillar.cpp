// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "caterpillar.hpp"
#include "caterpillar_instructions.hpp"
#include "vm.hpp"
#include <evmc/instructions.h>
#include <memory>

namespace evmone::caterpillar
{
namespace
{
using InstrTable = std::array<InstrFn, 256>;
template <evmc_revision Rev>
constexpr InstrTable build_instr_table() noexcept
{
#define X(OPCODE, IDENTIFIER) (instr::traits[OPCODE].since <= Rev ? OPCODE##_ : cat_undefined),
#define X_UNDEFINED(OPCODE) cat_undefined,
    return {MAP_OPCODE_TO_IDENTIFIER};
#undef X
#undef X_UNDEFINED
}

constexpr InstrTable instr_table[] = {
    build_instr_table<EVMC_FRONTIER>(),
    build_instr_table<EVMC_HOMESTEAD>(),
    build_instr_table<EVMC_TANGERINE_WHISTLE>(),
    build_instr_table<EVMC_SPURIOUS_DRAGON>(),
    build_instr_table<EVMC_BYZANTIUM>(),
    build_instr_table<EVMC_CONSTANTINOPLE>(),
    build_instr_table<EVMC_PETERSBURG>(),
    build_instr_table<EVMC_ISTANBUL>(),
    build_instr_table<EVMC_BERLIN>(),
    build_instr_table<EVMC_LONDON>(),
    build_instr_table<EVMC_SHANGHAI>(),
};
}  // namespace

evmc_result execute(
    const VM& /*vm*/, ExecutionState& state, const baseline::CodeAnalysis& analysis) noexcept
{
    state.analysis.baseline = &analysis;  // Assign code analysis for instruction implementations.

    // Use padded code.
    state.code = {analysis.padded_code.get(), state.code.size()};

    const auto& tbl = instr_table[state.rev];
    state.tbl = tbl.data();

    const auto code_it = state.code.data();
    const auto first_fn = tbl[*code_it];
    state.stack_bottom = state.stack.top_item;
    auto stack_top = state.stack.top_item;
    const auto status = first_fn(stack_top, code_it, state.gas_left, state);

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
