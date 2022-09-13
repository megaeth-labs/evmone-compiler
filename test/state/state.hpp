// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "account.hpp"
#include "hash_utils.hpp"
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace evmone::state
{
struct JournalBalanceChange
{
    address addr;
    intx::uint256 prev_balance;
};

struct JournalTouched
{
    address addr;
};

struct JournalStorageChange
{
    address addr;
    bytes32 key;
    bytes32 prev_value;
    evmc_access_status prev_access_status;
};

using JournalEntry = std::variant<JournalBalanceChange, JournalTouched, JournalStorageChange>;

class State
{
    std::unordered_map<address, Account> m_accounts;

    std::vector<JournalEntry> m_journal;

public:
    /// Creates new account under the address.
    Account& create(const address& addr)
    {
        const auto r = m_accounts.insert({addr, {}});
        assert(r.second);
        return r.first->second;
    }

    Account& get(const address& addr)
    {
        assert(m_accounts.count(addr) == 1);
        return m_accounts.find(addr)->second;
    }

    Account* get_or_null(const address& addr)
    {
        const auto it = m_accounts.find(addr);
        if (it != m_accounts.end())
            return &it->second;
        return nullptr;
    }

    Account& get_or_create(const address& addr) { return m_accounts[addr]; }

    auto& get_accounts() { return m_accounts; }

    void journal_balance_change(const address& addr, const intx::uint256& prev_balance)
    {
        m_journal.emplace_back(JournalBalanceChange{addr, prev_balance});
    }

    void journal_touched(const address& addr) { m_journal.emplace_back(JournalTouched{addr}); }

    void journal_storage_change(const address& addr, const bytes32& key, const StorageValue& value)
    {
        m_journal.emplace_back(JournalStorageChange{addr, key, value.current, value.access_status});
    }
};

struct BlockInfo
{
    int64_t number = 0;
    int64_t timestamp = 0;
    int64_t gas_limit = 0;
    address coinbase;
    bytes32 prev_randao;
    uint64_t base_fee = 0;
};

using AccessList = std::vector<std::pair<address, std::vector<bytes32>>>;

struct Transaction
{
    enum class Kind
    {
        legacy,
        eip1559
    };

    Kind kind = Kind::legacy;
    bytes data;
    int64_t gas_limit;
    intx::uint256 max_gas_price;
    intx::uint256 max_priority_gas_price;
    address sender;
    std::optional<address> to;
    intx::uint256 value;
    AccessList access_list;
};

struct Log
{
    address addr;
    bytes data;
    std::vector<hash256> topics;
};


[[nodiscard]] std::optional<std::vector<Log>> transition(
    State& state, const BlockInfo& block, const Transaction& tx, evmc_revision rev, evmc::VM& vm);

}  // namespace evmone::state
