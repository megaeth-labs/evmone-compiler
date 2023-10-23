#include <cstdio>
#include <string>

#include <evmc/evmc.hpp>
#include <evmc/mocked_host.hpp>
#include <evmone/evmone.h>
#include <evmone/baseline.hpp>
#include <evmone/execution_state.hpp>
#include <evmone/vm.hpp>
#include "../CLI11.hpp"
#include "Cycles.hpp"

int main(int argc, char** argv)
{
    std::string hex_code;
    std::string hex_calldata;
    int64_t gas = 1000000000000ll;
    uint8_t evmc_rev = evmc_revision::EVMC_SHANGHAI;
    CLI::App app{"interpreter"};
    app.add_option("--contract-code", hex_code,
           "Runtime contract code in hex format")->required();
    app.add_option("--calldata", hex_calldata,
           "Calldata in hex format")->required();
    app.add_option("--gas", gas,
           "Gas available for execution (default: 10^12)");
    app.add_option("--evm-revision", evmc_rev,
           "Revision number of the EVM specification (default: 12 [SHANGHAI])")
           ->check(CLI::Range(0, int(evmc_revision::EVMC_MAX_REVISION)));
    CLI11_PARSE(app, argc, argv)

    auto bytecode = evmc::from_hex(hex_code);
    auto calldata = evmc::from_hex(hex_calldata);
    if (!bytecode) {
        printf("Failed to parse the contract code!\n");
        return 0;
    } else if (!calldata) {
        printf("Failed to parse the calldata!\n");
        return 0;
    }

    const evmc_message msg {
        .gas = gas,
        .input_data = calldata.value().c_str(),
        .input_size = calldata.value().size()
    };
    const auto code_analysis = evmone::baseline::analyze(evmc_revision(evmc_rev), bytecode.value());
    const auto data = code_analysis.eof_header.get_data(bytecode.value());

    auto vm = static_cast<evmone::VM*>(evmc_create_evmone());
    evmc::MockedHost host;
    evmone::ExecutionState state(msg, evmc_revision(evmc_rev), host.get_interface(),
            host.to_context(), bytecode.value(), data);
    auto cyc = PerfUtils::Cycles::rdtsc();
    auto result = evmone::baseline::execute(*vm, msg.gas, state, code_analysis);
    cyc = PerfUtils::Cycles::rdtsc() - cyc;
    printf("ret_code = %d, gas_left = %ld, elapsed = %lu ms\n", result.status_code, result.gas_left,
        PerfUtils::Cycles::toMilliseconds(cyc));
}
