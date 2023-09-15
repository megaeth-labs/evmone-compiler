// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../state/state.hpp"
#include <evmc/evmc.hpp>
#include <vector>

#include <nlohmann/json.hpp>
namespace json = nlohmann;

namespace evmone::test
{

struct TestBlock
{
    state::BlockInfo block_info;
    state::State pre_state;
    std::vector<state::Transaction> transactions;
};

TestBlock load_test_block(const json::json& j);

struct BlockchainTransitionTest
{
    struct Case
    {
        std::string name;
        std::vector<TestBlock> test_blocks;
        struct Expectation
        {
            hash256 last_block_hash;
            state::State post_state;
        };

        state::BlockInfo genesis_block_header;
        state::State pre_state;
        evmc_revision rev;

        Expectation expectation;
    };

    std::vector<Case> cases;
};

BlockchainTransitionTest load_blockchain_test(const json::json& j);

}  // namespace evmone::test
