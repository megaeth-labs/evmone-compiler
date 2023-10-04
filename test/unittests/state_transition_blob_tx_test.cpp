// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, blob_tx_insuficient_funds)
{
    tx.to = To;
    tx.gas_limit = 25000;
    tx.max_gas_price = 1;
    tx.max_priority_gas_price = 0;
    tx.nonce = 1;
    tx.type = Transaction::Type::blob;
    tx.blob_hashes.emplace_back(
        0x0100000000000000000000000000000000000000000000000000000000000000_bytes32);
    tx.max_blob_gas_price = 1;
    block.base_fee = 1;

    pre.get_accounts()[tx.sender].balance = 0x20000 + 25000;

    rev = EVMC_CANCUN;

    expect.post.at(Coinbase).exists = false;
    expect.status = EVMC_SUCCESS;
}
