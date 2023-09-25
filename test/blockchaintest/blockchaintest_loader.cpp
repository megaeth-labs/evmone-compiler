// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../statetest/statetest.hpp"
#include "blockchaintest.hpp"

namespace evmone::test
{

namespace
{
template <typename T>
T load_if_exists(const json::json& j, std::string_view key)
{
    if (const auto it = j.find(key); it != j.end())
        return from_json<T>(*it);
    return {};
}
}  // namespace

template <>
BlockHeader from_json<BlockHeader>(const json::json& j)
{
    return {from_json<hash256>(j.at("parentHash")), from_json<address>(j.at("coinbase")),
        from_json<hash256>(j.at("stateRoot")), from_json<hash256>(j.at("receiptTrie")),
        state::bloom_filter_from_bytes(from_json<bytes>(j.at("bloom"))),
        load_if_exists<int64_t>(j, "difficulty"), load_if_exists<bytes32>(j, "mixHash"),
        from_json<int64_t>(j.at("number")), from_json<int64_t>(j.at("gasLimit")),
        from_json<int64_t>(j.at("gasUsed")), from_json<int64_t>(j.at("timestamp")),
        from_json<bytes>(j.at("extraData")), load_if_exists<uint64_t>(j, "baseFeePerGas"),
        from_json<hash256>(j.at("hash")), from_json<hash256>(j.at("transactionsTrie")),
        load_if_exists<hash256>(j, "withdrawalsRoot")};
}

static TestBlock load_test_block(const json::json& j, evmc_revision rev)
{
    using namespace state;
    TestBlock tb;

    if (const auto it = j.find("blockHeader"); it != j.end())
    {
        tb.block_info = from_json<BlockInfo>(*it);
        tb.expected_block_header = from_json<BlockHeader>(*it);
        tb.block_info.number = tb.expected_block_header.block_number;
        tb.block_info.timestamp = tb.expected_block_header.timestamp;
        tb.block_info.gas_limit = tb.expected_block_header.gas_limit;
        tb.block_info.coinbase = tb.expected_block_header.coinbase;
        tb.block_info.difficulty = tb.expected_block_header.difficulty;
        tb.block_info.prev_randao = tb.expected_block_header.prev_randao;
        tb.block_info.base_fee = tb.expected_block_header.base_fee_per_gas;

        // Override prev_randao with difficulty pre-Merge
        if (rev < EVMC_PARIS)
        {
            tb.block_info.prev_randao =
                intx::be::store<bytes32>(intx::uint256{tb.block_info.difficulty});
        }
    }
    else
    {
        // TODO: Add support for invalid blocks.
        throw std::runtime_error("Add support for invalid blocks.");
    }

    if (const auto it = j.find("uncleHeaders"); it != j.end())
    {
        auto current_block_number = tb.block_info.number;
        for (const auto& ommer : *it)
        {
            tb.block_info.ommers.push_back({from_json<address>(ommer.at("coinbase")),
                static_cast<uint32_t>(
                    current_block_number - from_json<int64_t>(ommer.at("number")))});
        }
    }


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
    c.genesis_block_header = from_json<BlockHeader>(j.at("genesisBlockHeader"));
    c.pre_state = from_json<State>(j.at("pre"));
    c.rev = to_rev(j.at("network").get<std::string>());

    for (const auto& el : j.at("blocks"))
        c.test_blocks.emplace_back(load_test_block(el, c.rev));

    c.expectation.last_block_hash = from_json<hash256>(j.at("lastblockhash"));

    if (const auto it = j.find("postState"); it != j.end())
        c.expectation.post_state = from_json<State>(*it);
    else if (const auto it_hash = j.find("postStateHash"); it_hash != j.end())
        c.expectation.post_state = from_json<hash256>(*it_hash);

    return c;
}
}  // namespace

static void from_json(const json::json& j, BlockchainTransitionTest& o)
{
    for (auto elem_it : j.items())
        o.cases.emplace_back(load_blockchain_test_case(elem_it.key(), elem_it.value()));
}

BlockchainTransitionTest load_blockchain_test(std::istream& input)
{
    return json::json::parse(input).get<BlockchainTransitionTest>();
}

}  // namespace evmone::test
