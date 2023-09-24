// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../state/bloom_filter.hpp"
#include "../state/state.hpp"
#include <evmc/evmc.hpp>
#include <vector>

#include <nlohmann/json.hpp>
namespace json = nlohmann;

namespace evmone::test
{

// https://ethereum.org/en/developers/docs/blocks/
struct BlockHeader
{
    hash256 parent_hash;
    address coinbase;
    hash256 state_root;
    hash256 receipts_root;
    state::BloomFilter logs_bloom;
    int64_t difficulty;
    bytes32 prev_randao;
    int64_t block_number;
    int64_t gas_limit;
    int64_t gas_used;
    int64_t timestamp;
    bytes extra_data;
    uint64_t base_fee_per_gas;
    hash256 hash;
    hash256 transactions_root;
    hash256 withdrawal_root;
    hash256 parent_beacon_block_root;
};

struct TestBlock
{
    state::BlockInfo block_info;
    state::State pre_state;
    std::vector<state::Transaction> transactions;

    BlockHeader expected_block_header;
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
            std::variant<state::State, hash256> post_state;
        };

        BlockHeader genesis_block_header;
        state::State pre_state;
        evmc_revision rev;

        Expectation expectation;
    };

    std::vector<Case> cases;
};

BlockchainTransitionTest load_blockchain_test(std::istream& input);

void run_blockchain_test(const BlockchainTransitionTest& test, evmc::VM& vm);

}  // namespace evmone::test
