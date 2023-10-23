#include <cstdio>
#include <cstdint>
#include <string>
#include <cassert>
#include <cstring>
#include <cstdlib>

#include "../Cycles.hpp"
#include "aot_compiler.hpp"

using namespace evmone;

/*
  EVM revision: Shanghai
  contract hex code: 5f35600060015b8215601b578181019150909160019003916006565b91505000
*/
evmc_result contract_0x3b2446dffe1de3628f8e1959888d96b25bcf72b5012af4dbb83ecbf17fd9570d(ExecutionState& state)
{
PROLOGUE
constexpr JumpdestMap jumpdest_map {2, {6,27}, {&&L_OFFSET_6,&&L_OFFSET_27}};

BLOCK_START(0, 11, 0, 3)
INVOKE(PUSH0)
INVOKE(CALLDATALOAD)
INVOKE(PUSH1, 0x0_u256)
INVOKE(PUSH1, 0x1_u256)

BLOCK_START(6, 20, 3, 2)
INVOKE(JUMPDEST)
INVOKE(DUP3)
INVOKE(ISZERO)
PUSHnJUMPI(27)

BLOCK_START(12, 43, 3, 2)
INVOKE(DUP2)
INVOKE(DUP2)
INVOKE(ADD)
INVOKE(SWAP2)
INVOKE(POP)
INVOKE(SWAP1)
INVOKE(SWAP2)
INVOKE(PUSH1, 0x1_u256)
INVOKE(SWAP1)
INVOKE(SUB)
INVOKE(SWAP2)
PUSHnJUMP(6)

BLOCK_START(27, 8, 3, 0)
INVOKE(JUMPDEST)
INVOKE(SWAP2)
INVOKE(POP)
INVOKE(POP)
INVOKE(STOP)

EPILOGUE
}

/*
  EVM revision: Shanghai
  contract hex code: 5f35600060015b8215601f575b818101915090916001900391821515600c575b91505000
*/
evmc_result contract_0xb5b6fa5a4a33eb343fc84af28dca9b1a79bb65f6cf0aee90e9d3f9f7f527f6b1(ExecutionState& state)
{
PROLOGUE
constexpr JumpdestMap jumpdest_map {3, {6,12,31}, {&&L_OFFSET_6,&&L_OFFSET_12,&&L_OFFSET_31}};

BLOCK_START(0, 11, 0, 3)
INVOKE(PUSH0)
INVOKE(CALLDATALOAD)
INVOKE(PUSH1, 0x0_u256)
INVOKE(PUSH1, 0x1_u256)

BLOCK_START(6, 20, 3, 2)
INVOKE(JUMPDEST)
INVOKE(DUP3)
INVOKE(ISZERO)
PUSHnJUMPI(31)

BLOCK_START(12, 55, 3, 2)
INVOKE(JUMPDEST)
INVOKE(DUP2)
INVOKE(DUP2)
INVOKE(ADD)
INVOKE(SWAP2)
INVOKE(POP)
INVOKE(SWAP1)
INVOKE(SWAP2)
INVOKE(PUSH1, 0x1_u256)
INVOKE(SWAP1)
INVOKE(SUB)
INVOKE(SWAP2)
INVOKE(DUP3)
INVOKE(ISZERO)
INVOKE(ISZERO)
PUSHnJUMPI(12)

BLOCK_START(31, 8, 3, 0)
INVOKE(JUMPDEST)
INVOKE(SWAP2)
INVOKE(POP)
INVOKE(POP)
INVOKE(STOP)

EPILOGUE
}

/*
  EVM revision: Shanghai
  contract hex code: 6080604052348015600f57600080fd5b506004361060285760003560e01c806361047ff414602d575b600080fd5b603c6038366004607d565b604e565b60405190815260200160405180910390f35b60006002821015605c575090565b6000600160025b84811160755790918201906001016063565b509392505050565
b600060208284031215608e57600080fd5b503591905056
*/
evmc_result contract_0x4caeaf714b12f4f7b28a334532c89f43b1fa92009d4171a2b49563858d85c499(ExecutionState& state)
{
PROLOGUE
constexpr JumpdestMap jumpdest_map {11, {15,40,45,56,60,78,92,99,117,125,142}, {&&L_OFFSET_15,&&L_OFFSET_40,&&L_OFFSET_45,&&L_OFFSET_56,&&L_OFFSET_60,&&L_OFFSET_78,&&L_OFFSET_92,&&L_OFFSET_99,&&L_OFFSET_117,&&L_OFFSET_125,&&L_OFFSET_142}};

BLOCK_START(0, 30, 0, 3)
INVOKE(PUSH1, 0x80_u256)
INVOKE(PUSH1, 0x40_u256)
INVOKE(MSTORE)
INVOKE(CALLVALUE)
INVOKE(DUP1)
INVOKE(ISZERO)
PUSHnJUMPI(15)

BLOCK_START(11, 6, 0, 2)
INVOKE(PUSH1, 0x0_u256)
INVOKE(DUP1)
INVOKE(REVERT)

BLOCK_START(15, 24, 1, 1)
INVOKE(JUMPDEST)
INVOKE(POP)
INVOKE(PUSH1, 0x4_u256)
INVOKE(CALLDATASIZE)
INVOKE(LT)
PUSHnJUMPI(40)

BLOCK_START(24, 34, 0, 3)
INVOKE(PUSH1, 0x0_u256)
INVOKE(CALLDATALOAD)
INVOKE(PUSH1, 0xe0_u256)
INVOKE(SHR)
INVOKE(DUP1)
INVOKE(PUSH4, 0x61047ff4_u256)
INVOKE(EQ)
PUSHnJUMPI(45)

BLOCK_START(40, 7, 0, 2)
INVOKE(JUMPDEST)
INVOKE(PUSH1, 0x0_u256)
INVOKE(DUP1)
INVOKE(REVERT)

BLOCK_START(45, 23, 0, 5)
INVOKE(JUMPDEST)
INVOKE(PUSH1, 0x3c_u256)
INVOKE(PUSH1, 0x38_u256)
INVOKE(CALLDATASIZE)
INVOKE(PUSH1, 0x4_u256)
PUSHnJUMP(125)

BLOCK_START(56, 12, 0, 1)
INVOKE(JUMPDEST)
PUSHnJUMP(78)

BLOCK_START(60, 40, 1, 2)
INVOKE(JUMPDEST)
INVOKE(PUSH1, 0x40_u256)
INVOKE(MLOAD)
INVOKE(SWAP1)
INVOKE(DUP2)
INVOKE(MSTORE)
INVOKE(PUSH1, 0x20_u256)
INVOKE(ADD)
INVOKE(PUSH1, 0x40_u256)
INVOKE(MLOAD)
INVOKE(DUP1)
INVOKE(SWAP2)
INVOKE(SUB)
INVOKE(SWAP1)
INVOKE(RETURN)

BLOCK_START(78, 29, 1, 3)
INVOKE(JUMPDEST)
INVOKE(PUSH1, 0x0_u256)
INVOKE(PUSH1, 0x2_u256)
INVOKE(DUP3)
INVOKE(LT)
INVOKE(ISZERO)
PUSHnJUMPI(92)

BLOCK_START(89, 13, 3, 0)
INVOKE(POP)
INVOKE(SWAP1)
INVOKE(JUMP, jumpdest_map)

BLOCK_START(92, 10, 0, 3)
INVOKE(JUMPDEST)
INVOKE(PUSH1, 0x0_u256)
INVOKE(PUSH1, 0x1_u256)
INVOKE(PUSH1, 0x2_u256)

BLOCK_START(99, 23, 5, 2)
INVOKE(JUMPDEST)
INVOKE(DUP5)
INVOKE(DUP2)
INVOKE(GT)
PUSHnJUMPI(117)

BLOCK_START(106, 32, 3, 1)
INVOKE(SWAP1)
INVOKE(SWAP2)
INVOKE(DUP3)
INVOKE(ADD)
INVOKE(SWAP1)
INVOKE(PUSH1, 0x1_u256)
INVOKE(ADD)
PUSHnJUMP(99)

BLOCK_START(117, 23, 6, 0)
INVOKE(JUMPDEST)
INVOKE(POP)
INVOKE(SWAP4)
INVOKE(SWAP3)
INVOKE(POP)
INVOKE(POP)
INVOKE(POP)
INVOKE(JUMP, jumpdest_map)

BLOCK_START(125, 35, 2, 4)
INVOKE(JUMPDEST)
INVOKE(PUSH1, 0x0_u256)
INVOKE(PUSH1, 0x20_u256)
INVOKE(DUP3)
INVOKE(DUP5)
INVOKE(SUB)
INVOKE(SLT)
INVOKE(ISZERO)
PUSHnJUMPI(142)

BLOCK_START(138, 6, 0, 2)
INVOKE(PUSH1, 0x0_u256)
INVOKE(DUP1)
INVOKE(REVERT)

BLOCK_START(142, 22, 4, 0)
INVOKE(JUMPDEST)
INVOKE(POP)
INVOKE(CALLDATALOAD)
INVOKE(SWAP2)
INVOKE(SWAP1)
INVOKE(POP)
INVOKE(JUMP, jumpdest_map)

EPILOGUE
}

/// Native implementation of fibonacci sequence
uint256 native_c_fib(int64_t N) {
    uint256 fi;
    uint256 f1 = 0;
    uint256 f2 = 1;
    for (int64_t i = 2; i <= N; i++) {
        fi = f1 + f2;
        f1 = f2;
        f2 = fi;
    }
    return f2;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: fib [N]\n");
        return 0;
    }

    printf("Compiler config:\n");
    printf("  ENABLED_PUSHnJUMP: %s\n", ENABLE_PUSHnJUMP ? "true" : "false");
    printf("  GAS_CHECK_LOC:     %d\n", GAS_CHECK_LOC);
    printf("  GAS_CHECK_OFF:     %s\n", GAS_CHECK_OFF ? "true" : "false");

    const int64_t N = std::stoll(argv[1]);
    printf("\nComputing fib(%ld)\n", N);

    using namespace PerfUtils;
    uint64_t baseline_time = Cycles::rdtsc();
    [[maybe_unused]] auto volatile x = native_c_fib(N);
    baseline_time = Cycles::rdtsc() - baseline_time;
    printf("=== native-fib ===\nelapsed %lu ms\n\n", Cycles::toMilliseconds(baseline_time));

    const std::string names[] {"fib", "fib-loop-inv", "fib-solidity"};
    for (const int experiment : {0, 1, 2}) {
        ExecutionState state;
        evmc_result result;

        evmc::bytes calldata;
        if (experiment == 2) {
            calldata.assign({0x61, 0x04, 0x7f, 0xf4});
        }
        calldata.insert(calldata.size(), 24, 0);
        for (size_t i = 0; i < 64; i += 8) {
            calldata.push_back(uint8_t(N >> (56 - i)));
        }

        evmc_message msg {
            .gas = 1000000000000ll,
            .input_data = calldata.c_str(),
            .input_size = calldata.size()
        };
        state.msg = &msg;

        printf("=== %s ===\n", names[experiment].c_str());
        uint64_t tsc = Cycles::rdtsc();
        if (experiment == 0) {
            // Hand-coded standalone fibonacci bytecode sequence
            result = contract_0x3b2446dffe1de3628f8e1959888d96b25bcf72b5012af4dbb83ecbf17fd9570d(state);
        } else if (experiment == 1) {
            // Hand-coded standalone fibonacci bytecode sequence w/ manual loop inversion
            result = contract_0xb5b6fa5a4a33eb343fc84af28dca9b1a79bb65f6cf0aee90e9d3f9f7f527f6b1(state);
        } else {
            // Fibonacci contract bytecode generated by the solidity compiler
            result = contract_0x4caeaf714b12f4f7b28a334532c89f43b1fa92009d4171a2b49563858d85c499(state);
        }
        tsc = Cycles::rdtsc() - tsc;

        printf("calldata: %s\n", evmc::hex(calldata).c_str());
        printf("ret_code = %d, gas_left = %ld, elapsed = %lu ms, slowdown = %.2fx\n\n",
            result.status_code, result.gas_left, Cycles::toMilliseconds(tsc),
            double(tsc) / double(baseline_time));
    }
}
