// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state.hpp"
#include "host.hpp"
#include <evmone/evmone.h>
#include <evmone/execution_state.hpp>

namespace stdx
{
/// Implementation of std::erase_if from C++20.
/// Taken from https://en.cppreference.com/w/cpp/container/unordered_map/erase_if.
template <class Key, class T, class Hash, class KeyEqual, class Alloc, class Pred>
typename std::unordered_map<Key, T, Hash, KeyEqual, Alloc>::size_type erase_if(
    std::unordered_map<Key, T, Hash, KeyEqual, Alloc>& c, Pred pred)
{
    auto old_size = c.size();
    for (auto i = c.begin(), last = c.end(); i != last;)
    {
        if (pred(*i))
            i = c.erase(i);
        else
            ++i;
    }
    return old_size - c.size();
}
}  // namespace stdx

namespace evmone::state
{
namespace
{
int64_t compute_tx_data_cost(evmc_revision rev, bytes_view data) noexcept
{
    constexpr int64_t zero_byte_cost = 4;
    const int64_t nonzero_byte_cost = rev >= EVMC_ISTANBUL ? 16 : 68;
    int64_t cost = 0;
    for (const auto b : data)
        cost += (b == 0) ? zero_byte_cost : nonzero_byte_cost;
    return cost;
}

int64_t compute_access_list_cost(const AccessList& access_list) noexcept
{
    static constexpr auto storage_key_cost = 1900;
    static constexpr auto address_cost = 2400;

    int64_t cost = 0;
    for (const auto& a : access_list)
        cost += address_cost + static_cast<int64_t>(a.second.size()) * storage_key_cost;
    return cost;
}

int64_t compute_tx_intrinsic_cost(evmc_revision rev, const Transaction& tx) noexcept
{
    static constexpr auto call_tx_cost = 21000;
    static constexpr auto create_tx_cost = 53000;
    const bool is_create = !tx.to.has_value();
    const auto tx_cost = is_create && rev >= EVMC_HOMESTEAD ? create_tx_cost : call_tx_cost;
    return tx_cost + compute_tx_data_cost(rev, tx.data) + compute_access_list_cost(tx.access_list);
}

evmc_message build_message(const Transaction& tx, int64_t execution_gas_limit) noexcept
{
    const auto recipient = tx.to.has_value() ? *tx.to : evmc::address{};
    return {
        tx.to.has_value() ? EVMC_CALL : EVMC_CREATE,
        0,
        0,
        execution_gas_limit,
        recipient,
        tx.sender,
        tx.data.data(),
        tx.data.size(),
        intx::be::store<evmc::uint256be>(tx.value),
        {},
        recipient,
    };
}
}  // namespace

void State::journal_rollback(size_t checkpoint) noexcept
{
    while (m_journal.size() != checkpoint)
    {
        std::visit(
            [this](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, JournalNonceBump>)
                    e.a->nonce -= 1;
                else if constexpr (std::is_same_v<T, JournalTouched>)
                    e.a->touched = false;
                else if constexpr (std::is_same_v<T, JournalCreate>)
                {
                    auto& a = get(e.addr);
                    a.nonce = 0;
                    a.code.clear();
                    if (!e.existed)
                        m_accounts.erase(e.addr);
                }
                else if constexpr (std::is_same_v<T, JournalStorageChange>)
                {
                    e.p->second.current = e.prev_value;
                    e.p->second.access_status = e.prev_access_status;
                }
                else if constexpr (std::is_same_v<T, JournalBalanceChange>)
                {
                    e.a->balance = e.prev_balance;
                }
            },
            m_journal.back());
        m_journal.pop_back();
    }
}

std::optional<std::vector<Log>> transition(
    State& state, const BlockInfo& block, const Transaction& tx, evmc_revision rev, evmc::VM& vm)
{
    if (rev < EVMC_LONDON && tx.kind == Transaction::Kind::eip1559)
        return {};

    if (rev < EVMC_BERLIN && !tx.access_list.empty())
        return {};

    if (tx.max_gas_price < tx.max_priority_gas_price)
        return {};  // tip too high

    if (block.gas_limit < tx.gas_limit)
        return {};

    const auto base_fee = (rev >= EVMC_LONDON) ? block.base_fee : 0;
    if (tx.max_gas_price < base_fee)
        return {};

    auto& sender_acc = state.get(tx.sender);
    if (!sender_acc.code.empty())
        return {};  // Tx origin must not be a contract (EIP-3607).

    const auto tx_max_cost_512 = intx::uint512{tx.gas_limit} * intx::uint512{tx.max_gas_price};
    auto sender_balance_check = sender_acc.balance;
    if (sender_balance_check < tx_max_cost_512)
        return {};

    sender_balance_check -= static_cast<intx::uint256>(tx_max_cost_512);

    const auto execution_gas_limit = tx.gas_limit - compute_tx_intrinsic_cost(rev, tx);
    if (execution_gas_limit < 0)
        return {};

    assert(tx.max_gas_price >= base_fee);                   // Checked at the front.
    assert(tx.max_gas_price >= tx.max_priority_gas_price);  // Checked at the front.
    const auto priority_gas_price =
        std::min(tx.max_priority_gas_price, tx.max_gas_price - base_fee);
    const auto effective_gas_price = base_fee + priority_gas_price;

    assert(effective_gas_price <= tx.max_gas_price);
    const auto tx_max_cost = tx.gas_limit * effective_gas_price;
    auto sender_balance = sender_acc.balance - tx_max_cost;

    if (sender_balance_check < tx.value)
        return {};

    // Bump sender nonce.
    // This must be the last transaction validity check, because it modifies the state.
    if (const auto nonce_ok = sender_acc.bump_nonce(); !nonce_ok)
        return {};

    sender_acc.balance = sender_balance;  // Modify sender balance after all checks.

    Host host{rev, vm, state, block, tx};

    const auto result = host.call(build_message(tx, execution_gas_limit));

    auto gas_used = tx.gas_limit - result.gas_left;

    const auto max_refund_quotient = rev >= EVMC_LONDON ? 5 : 2;
    const auto refund_limit = gas_used / max_refund_quotient;
    const auto refund = std::min(result.gas_refund, refund_limit);
    gas_used -= refund;

    const auto sender_fee = gas_used * effective_gas_price;
    const auto producer_pay = gas_used * priority_gas_price;

    sender_acc.balance += tx_max_cost - sender_fee;

    auto& coinbase_acc = state.get_or_create(block.coinbase);
    coinbase_acc.balance += producer_pay;
    coinbase_acc.touched = true;

    auto& accounts = state.get_accounts();

    // Apply destructs.
    for (const auto& addr : host.get_destructs())
        accounts.erase(addr);

    if (rev >= EVMC_SPURIOUS_DRAGON)
    {
        // Clear touched empty accounts.
        stdx::erase_if(accounts, [](const std::pair<const address, Account>& p) noexcept {
            return p.second.touched && p.second.is_empty();
        });
    }

    return host.take_logs();
}
}  // namespace evmone::state
