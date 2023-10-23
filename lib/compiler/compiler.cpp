#include <cstdio>
#include <cstdint>
#include <string>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <fstream>

#include <evmc/hex.hpp>

#include "CLI11.hpp"
#include "aot_compiler.hpp"

using namespace evmone;

/// Record information about a basic block that are useful at compile time.
struct BasicBlockAnalysis {
    /// True iff. this basic block contains no OP_INVALID.
    bool valid = true;

    /// Starting offset of this basic block.
    size_t start_offset;

    /// Cost table for the opcodes.
    const baseline::CostTable& cost_table;

    /// Opcodes in this block.
    std::vector<Opcode> opcodes {};

    /// Immediate values in this basic block.
    std::vector<std::optional<uint256>> imm_values {};

    /// push_n_jump[i] is true if opcodes[i:i+2] has a "PUSH & JUMP" pattern.
    std::vector<bool> push_n_jump;

    /// Base gas costs of this basic block.
    int64_t base_gas_cost {};

    /// Minimum stack height required by this basic block (to avoid stack underflow).
    int stack_required {};

    /// Growth in stack height after executing this basic block (may be negative).
    int stack_max_growth {};

    explicit BasicBlockAnalysis(size_t offset, const baseline::CostTable& cost_table)
        : start_offset(offset)
        , cost_table(cost_table)
    {}
};

/// Return the hex representation of a uint256.
std::string hex(uint256 x)
{
    if (x == 0)
        return "0";

    auto s = std::string{};
    while (x != 0)
    {
        const auto d = x % 16;
        const auto c = d < 10 ? '0' + d : 'a' + d - 10;
        s.push_back(char(c));
        x >>= 4;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

std::string compile_cxx(const evmc_revision rev, bytes_view code)
{
    const baseline::CostTable& cost_table =
        baseline::get_baseline_cost_table(rev, 0 /* legacy format: no RJUMP/DATALOADN/... */);

    std::vector<BasicBlockAnalysis> basic_blks;
    std::vector<size_t> jumpdests;

    bool create_new_bb = true;
    for (size_t i = 0; i < code.size(); ++i) {
        // Create a new basic block if (at least) one of the following is true:
        // - The previous opcode is STOP, RETURN, REVERT, SELFDESTRUCT, or JUMP(I)
        // - The current opcode is JUMPDEST
        const auto opcode = Opcode(code[i]);
        if (opcode == OP_JUMPDEST) {
            create_new_bb = true;
            jumpdests.push_back(i);
        }
        if (create_new_bb) {
            basic_blks.emplace_back(i, cost_table);
            create_new_bb = false;
        }

        // Grow the current basic block.
        auto& bb = basic_blks.back();
        auto trait = instr::traits[opcode];
        bb.opcodes.push_back(Opcode(opcode));
        if (trait.immediate_size > 0) {
            uint256 imm = 0;
            for (size_t k = i + 1; k <= i + trait.immediate_size; ++k)
                imm = imm << 8 | code[k];
            bb.imm_values.emplace_back(imm);
            i += trait.immediate_size;
        } else {
            bb.imm_values.emplace_back();
        }

        // Time to close the current basic block?
        switch (opcode)
        {
        case OP_STOP:
        case OP_RETURN:
        case OP_REVERT:
        case OP_SELFDESTRUCT:
        case OP_JUMP:
        case OP_JUMPI:
            create_new_bb = true;
            break;
        default:
            break;
        }
    }

    // Compute the summary of every basic block.
    for (auto& bb : basic_blks) {
        int stack_change = 0;
        bb.push_n_jump.assign(bb.opcodes.size(), false);
        for (size_t i = 0; i < bb.opcodes.size(); ++i) {
            const auto op = bb.opcodes[i];
            bb.base_gas_cost += cost_table[op];
            auto current_stack_required =
                instr::traits[op].stack_height_required - stack_change;
            bb.stack_required = std::max(bb.stack_required, current_stack_required);
            stack_change += instr::traits[op].stack_height_change;
            bb.stack_max_growth = std::max(bb.stack_max_growth, stack_change);
            bb.valid &= (op != OP_INVALID);

            if ((op == OP_JUMP || op == OP_JUMPI) && (i > 0)) {
                const auto prev_op = bb.opcodes[i - 1];
                bb.push_n_jump[i - 1] = (prev_op >= OP_PUSH0 && prev_op <= OP_PUSH32);
            }
        }
    }

    // Generate the C++ code snippet.
    std::string compiled;
    compiled += "/*\n  EVM revision: " + std::string(evmc_revision_to_string(rev)) +
                "\n  contract hex code: " + evmc::hex(code) + "\n*/\n";
    compiled +=
        "evmc_result contract_0x" + hex(intx::load_be256(ethash::keccak256(code.data(), code.size()))) +
        "(ExecutionState& state)\n{\n";
    compiled += "PROLOGUE\n";

    compiled += "constexpr JumpdestMap jumpdest_map {" + std::to_string(jumpdests.size());
    compiled += ", {";
    for (size_t jumpdest : jumpdests) {
        compiled += std::to_string(jumpdest) + ",";
    }
    compiled.pop_back();
    compiled += "}, {";
    for (size_t jumpdest : jumpdests) {
        compiled += "&&L_OFFSET_" + std::to_string(jumpdest) + ",";
    }
    compiled.pop_back();
    compiled += "}};\n";

    for (const auto& bb : basic_blks) {
        if (!bb.valid)
            continue;

        char buf[1024];
        std::ignore = std::sprintf(buf, "\nBLOCK_START(%lu, %ld, %d, %d)\n",
            bb.start_offset, bb.base_gas_cost, bb.stack_required, bb.stack_max_growth);
        compiled += buf;
        for (size_t i = 0; i < bb.opcodes.size(); ++i) {
            uint256 imm;
            if (bb.push_n_jump[i]) {
                imm = bb.imm_values[i] ? *bb.imm_values[i] : 0;
                assert(imm == uint64_t(imm));
                std::ignore = std::sprintf(buf, "PUSHn%s(%lu)\n",
                    instr::traits[bb.opcodes[i + 1]].name, uint64_t(imm));
                ++i;
            } else {
                const auto opcode = bb.opcodes[i];
                auto sz = std::sprintf(buf, "INVOKE(%s", instr::traits[opcode].name);
                if (bb.imm_values[i]) {
                    imm = *bb.imm_values[i];
                    sz += std::sprintf(buf + sz, ", 0x%s_u256", hex(imm).c_str());
                } else if (opcode == OP_JUMP || opcode == OP_JUMPI) {
                    sz += std::sprintf(buf + sz, ", jumpdest_map");
                }
                std::ignore = std::sprintf(buf + sz, ")\n");
            }
            compiled += buf;
        }
    }
    compiled += "\nEPILOGUE\n";
    compiled += "}\n";
    return compiled;
}

int main(int argc, char** argv)
{
    std::string hex_string;
    uint8_t evmc_rev = evmc_revision::EVMC_SHANGHAI;
    CLI::App app{"evm-compiler"};
    app.add_option("contract-code", hex_string,
            "Runtime contract code in hex format (no prefix 0x)")->required();
    app.add_option("--evm-revision", evmc_rev,
            "Revision number of the EVM specification (default: 12 [SHANGHAI])")
            ->check(CLI::Range(0, int(evmc_revision::EVMC_MAX_REVISION)));
    CLI11_PARSE(app, argc, argv)

    auto bytecode = evmc::from_hex(hex_string);
    if (!bytecode) {
        printf("Failed to parse the contract code!\n");
        return 0;
    }

    auto compiled = compile_cxx(evmc_revision(evmc_rev), bytecode.value());
    printf("%s\n", compiled.c_str());
}
