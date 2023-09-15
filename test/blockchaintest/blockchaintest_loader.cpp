// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../statetest/statetest.hpp"
#include "blockchaintest.hpp"

namespace evmone::test
{

TestBlock load_test_block(const json::json& j)
{
    using namespace state;
    TestBlock tb;

    if (auto it = j.find("blockHeader"); it != j.end())
        tb.block_info = from_json<BlockInfo>(*it);

    if (auto it = j.find("withdrawals"); it != j.end())
    {
        for (const auto& withdrawal : *it)
            tb.block_info.withdrawals.emplace_back(from_json<Withdrawal>(withdrawal));
    }

    if (auto it = j.find("transactions"); it != j.end())
    {
        for (const auto& tx : *it)
            tb.transactions.emplace_back(from_json<Transaction>(tx));
    }

    return tb;
}

namespace
{
BlockchainTransitionTest::Case load_blockchain_test_case(
    const std::string& name, const json::json& j)
{
    using namespace state;

    BlockchainTransitionTest::Case c;
    c.name = name;
    c.genesis_block_header = from_json<BlockInfo>(j.at("genesisBlockHeader"));
    c.pre_state = from_json<State>(j.at("pre"));
    c.rev = to_rev(j.at("network").get<std::string>());

    for (const auto& el : j.at("blocks"))
        c.test_blocks.emplace_back(load_test_block(el));

    c.expectation.last_block_hash = from_json<hash256>(j.at("lastblockhash"));
    c.expectation.post_state = from_json<State>(j.at("postState"));

    return c;
}
}  // namespace

BlockchainTransitionTest load_blockchain_test(const json::json& j)
{
    BlockchainTransitionTest btt;

    for (auto elem_it : j.items())
        btt.cases.emplace_back(load_blockchain_test_case(elem_it.key(), elem_it.value()));

    return btt;
}

}  // namespace evmone::test
