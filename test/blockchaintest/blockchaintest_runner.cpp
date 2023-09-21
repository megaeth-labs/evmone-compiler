// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../state/mpt_hash.hpp"
#include "../state/rlp.hpp"
#include "../state/state.hpp"
#include "../test/statetest/statetest.hpp"
#include "blockchaintest.hpp"
#include <gtest/gtest.h>

namespace evmone::test
{

struct RejectedTransaction
{
    hash256 hash;
    size_t index;
    std::string message;
};

struct TransitionResult
{
    std::vector<state::TransactionReceipt> receipts;
    std::vector<RejectedTransaction> rejected;
    int64_t gas_used;
};

TransitionResult apply_block(state::State& state, evmc::VM& vm, const state::BlockInfo& block,
    const std::vector<state::Transaction>& txs, evmc_revision rev,
    std::optional<int64_t> block_reward)
{
    std::vector<state::Log> txs_logs;
    int64_t block_gas_left = block.gas_limit;

    std::vector<RejectedTransaction> rejected_txs;
    std::vector<state::TransactionReceipt> receipts;
    std::vector<state::Transaction> transactions;

    int64_t cumulative_gas_used = 0;

    for (size_t i = 0; i < txs.size(); ++i)
    {
        const auto& tx = txs[i];
        // tx.chain_id = chain_id;

        const auto computed_tx_hash = keccak256(rlp::encode(tx));
        const auto computed_tx_hash_str = hex0x(computed_tx_hash);
        auto res = state::transition(state, block, tx, rev, vm, block_gas_left);

        if (holds_alternative<std::error_code>(res))
        {
            const auto ec = std::get<std::error_code>(res);
            rejected_txs.push_back({computed_tx_hash, i, ec.message()});
        }
        else
        {
            auto& receipt = get<state::TransactionReceipt>(res);

            const auto& tx_logs = receipt.logs;

            txs_logs.insert(txs_logs.end(), tx_logs.begin(), tx_logs.end());
            receipt.transaction_hash = computed_tx_hash;
            cumulative_gas_used += receipt.gas_used;
            receipt.cumulative_gas_used = cumulative_gas_used;
            if (rev < EVMC_BYZANTIUM)
                receipt.post_state = state::mpt_hash(state.get_accounts());

            transactions.emplace_back(std::move(tx));
            block_gas_left -= receipt.gas_used;
            receipts.emplace_back(std::move(receipt));
        }

        // j_result["logsHash"] = hex0x(logs_hash(txs_logs));
        // j_result["stateRoot"] = hex0x(state::mpt_hash(state.get_accounts()));
    }

    state::finalize(state, rev, block.coinbase, block_reward, block.ommers, block.withdrawals);

    return {receipts, rejected_txs, cumulative_gas_used};
}

std::optional<int64_t> mining_reward(evmc_revision rev)
{
    if (rev < EVMC_BYZANTIUM)
        return 5000000000000000000;
    else if (rev < EVMC_CONSTANTINOPLE)
        return 3000000000000000000;
    else if (rev < EVMC_PARIS)
        return 2000000000000000000;
    else
        return {};
}

void run_blockchain_test(const BlockchainTransitionTest& test, evmc::VM& vm)
{
    for (size_t case_index = 0; case_index != test.cases.size(); ++case_index)
    {
        const auto& c = test.cases[case_index];
        SCOPED_TRACE(
            std::string{evmc::to_string(c.rev)} + '/' + std::to_string(case_index) + '/' + c.name);

        auto state = c.pre_state;

        //        apply_block(state, vm, c.genesis_block_header, {}, c.rev, 0);
        //        EXPECT_EQ(
        //            state::mpt_hash(state.get_accounts()),
        //            state::mpt_hash(c.pre_state.get_accounts()));

        for (const auto& test_block : c.test_blocks)
        {
            const auto res = apply_block(state, vm, test_block.block_info, test_block.transactions,
                c.rev, mining_reward(c.rev));

            SCOPED_TRACE(std::string{evmc::to_string(c.rev)} + '/' + std::to_string(case_index) + '/' + c.name + '/' +
                         std::to_string(test_block.block_info.number));

            EXPECT_EQ(
                state::mpt_hash(state.get_accounts()), test_block.expected_block_header.state_root);
            if (c.rev >= EVMC_SHANGHAI)
            {
                EXPECT_EQ(state::mpt_hash(test_block.block_info.withdrawals),
                    test_block.expected_block_header.withdrawal_root);
            }

            EXPECT_EQ(state::mpt_hash(test_block.transactions),
                test_block.expected_block_header.transactions_root);
            EXPECT_EQ(
                state::mpt_hash(res.receipts), test_block.expected_block_header.receipts_root);
            EXPECT_EQ(res.gas_used, test_block.expected_block_header.gas_used);

            // TODO: Add difficulty calculation verification.
        }

        auto post_state_hash = std::holds_alternative<state::State>(c.expectation.post_state) ?
                state::mpt_hash(std::get<state::State>(c.expectation.post_state).get_accounts())
                                   : std::get<hash256>(c.expectation.post_state);
        EXPECT_EQ(state::mpt_hash(state.get_accounts()), post_state_hash);
    }
}
}  // namespace evmone::test
