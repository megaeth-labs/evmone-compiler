// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state.hpp"
#include "../utils/stdx/utility.hpp"
#include "errors.hpp"
#include "host.hpp"
#include "rlp.hpp"
#include <evmone/evmone.h>
#include <evmone/execution_state.hpp>

namespace evmone::state
{
namespace
{
constexpr auto GAS_PER_BLOB = 0x20000;

inline constexpr int64_t num_words(size_t size_in_bytes) noexcept
{
    return static_cast<int64_t>((size_in_bytes + 31) / 32);
}

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

int64_t fake_exponential(int64_t factor, int64_t numerator, int64_t denominator)
{
    int64_t i = 1;
    int64_t output = 0;
    auto numerator_accum = factor * denominator;
    while (numerator_accum > 0)
    {
        output += numerator_accum;
        numerator_accum = (numerator_accum * numerator) / (denominator * i);
        i += 1;
    }
    return output / denominator;
}

int64_t compute_tx_intrinsic_cost(evmc_revision rev, const Transaction& tx) noexcept
{
    static constexpr auto call_tx_cost = 21000;
    static constexpr auto create_tx_cost = 53000;
    static constexpr auto initcode_word_cost = 2;
    const auto is_create = !tx.to.has_value();
    const auto initcode_cost =
        is_create && rev >= EVMC_SHANGHAI ? initcode_word_cost * num_words(tx.data.size()) : 0;
    const auto tx_cost = is_create && rev >= EVMC_HOMESTEAD ? create_tx_cost : call_tx_cost;
    return tx_cost + compute_tx_data_cost(rev, tx.data) + compute_access_list_cost(tx.access_list) +
           initcode_cost;
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

/// Validates transaction and computes its execution gas limit (the amount of gas provided to EVM).
/// @return  Execution gas limit or transaction validation error.
std::variant<int64_t, std::error_code> validate_transaction(const Account& sender_acc,
    const BlockInfo& block, const Transaction& tx, evmc_revision rev, int64_t block_gas_left,
    int64_t blob_gas_left) noexcept
{
    const auto blob_gas_price = fake_exponential(1, block.excess_blob_gas, 3338477);

    switch (tx.type)
    {
    case Transaction::Type::blob:
        if (rev < EVMC_CANCUN)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
        if (!tx.to.has_value())
            return make_error_code(CREATE_BLOB_TX);
        if (tx.blob_hashes.empty())
            return make_error_code(EMPTY_BLOB_HASHES_LIST);
        if (tx.blob_hashes.size() > 6)
            return make_error_code(BLOB_HASHES_LIST_SIZE_LIMIT_EXCEEDED);

        if (tx.max_blob_gas_price < blob_gas_price)
            return make_error_code(FEE_CAP_LESS_THEN_BLOCKS);

        if (std::ranges::any_of(tx.blob_hashes, [](const auto& h) { return h.bytes[0] != 0x01; }))
            return make_error_code(INVALID_BLOB_HASH_VERSION);
        if (std::cmp_greater(GAS_PER_BLOB * tx.blob_hashes.size(), blob_gas_left))
            return make_error_code(BLOB_GAS_LIMIT_EXCEEDED);
        [[fallthrough]];

    case Transaction::Type::eip1559:
        if (rev < EVMC_LONDON)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);

        if (tx.max_priority_gas_price > tx.max_gas_price)
            return make_error_code(TIP_GT_FEE_CAP);  // Priority gas price is too high.
        [[fallthrough]];

    case Transaction::Type::access_list:
        if (rev < EVMC_BERLIN)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
        [[fallthrough]];

    case Transaction::Type::legacy:;
    }

    assert(tx.max_priority_gas_price <= tx.max_gas_price);

    if (tx.gas_limit > block_gas_left)
        return make_error_code(GAS_LIMIT_REACHED);

    if (tx.max_gas_price < block.base_fee)
        return make_error_code(FEE_CAP_LESS_THEN_BLOCKS);

    if (!sender_acc.code.empty())
        return make_error_code(SENDER_NOT_EOA);  // Origin must not be a contract (EIP-3607).

    if (sender_acc.nonce == Account::NonceMax)
        return make_error_code(NONCE_HAS_MAX_VALUE);

    if (sender_acc.nonce < tx.nonce)
        return make_error_code(NONCE_TOO_HIGH);

    if (sender_acc.nonce > tx.nonce)
        return make_error_code(NONCE_TOO_LOW);

    // initcode size is limited by EIP-3860.
    if (rev >= EVMC_SHANGHAI && !tx.to.has_value() && tx.data.size() > max_initcode_size)
        return make_error_code(INIT_CODE_SIZE_LIMIT_EXCEEDED);

    // Compute and check if sender has enough balance for the theoretical maximum transaction cost.
    // Note this is different from tx_max_cost computed with effective gas price later.
    // The computation cannot overflow if done with 512-bit precision.
    auto max_total_fee = umul(intx::uint256{tx.gas_limit}, tx.max_gas_price);
    max_total_fee += tx.value;

    if (tx.type == Transaction::Type::blob)
    {
        const auto total_blob_gas = GAS_PER_BLOB * tx.blob_hashes.size();
        // FIXME: Can overflow uint256.
        max_total_fee += total_blob_gas * tx.max_blob_gas_price;
    }
    if (sender_acc.balance < max_total_fee)
        return make_error_code(INSUFFICIENT_FUNDS);

    const auto execution_gas_limit = tx.gas_limit - compute_tx_intrinsic_cost(rev, tx);
    if (execution_gas_limit < 0)
        return make_error_code(INTRINSIC_GAS_TOO_LOW);

    return execution_gas_limit;
}

namespace
{
/// Deletes "touched" (marked as erasable) empty accounts in the state.
void delete_empty_accounts(State& state)
{
    std::erase_if(state.get_accounts(), [](const std::pair<const address, Account>& p) noexcept {
        const auto& acc = p.second;
        return acc.erasable && acc.is_empty();
    });
}
}  // namespace

void finalize(State& state, evmc_revision rev, const address& coinbase,
    std::optional<uint64_t> block_reward, std::span<const Ommer> ommers,
    std::span<const Withdrawal> withdrawals)
{
    // TODO: The block reward can be represented as a withdrawal.
    if (block_reward.has_value())
    {
        const auto reward = *block_reward;
        assert(reward % 32 == 0);  // Assume block reward is divisible by 32.
        const auto reward_by_32 = reward / 32;
        const auto reward_by_8 = reward / 8;

        state.touch(coinbase).balance += reward + reward_by_32 * ommers.size();
        for (const auto& ommer : ommers)
        {
            assert(ommer.delta > 0 && ommer.delta < 8);
            state.touch(ommer.beneficiary).balance += reward_by_8 * (8 - ommer.delta);
        }
    }

    for (const auto& withdrawal : withdrawals)
        state.touch(withdrawal.recipient).balance += withdrawal.get_amount();

    // Delete potentially empty block reward recipients.
    if (rev >= EVMC_SPURIOUS_DRAGON)
        delete_empty_accounts(state);
}

std::variant<TransactionReceipt, std::error_code> transition(State& state, const BlockInfo& block,
    const Transaction& tx, evmc_revision rev, evmc::VM& vm, int64_t block_gas_left,
    int64_t blob_gas_left)
{
    auto* sender_ptr = state.find(tx.sender);

    // Validate transaction. The validation needs the sender account, so in case
    // it doesn't exist provide an empty one. The account isn't created in the state
    // to prevent the state modification in case the transaction is invalid.
    const auto validation_result =
        validate_transaction((sender_ptr != nullptr) ? *sender_ptr : Account{}, block, tx, rev,
            block_gas_left, blob_gas_left);

    if (holds_alternative<std::error_code>(validation_result))
        return get<std::error_code>(validation_result);

    // Once the transaction is valid, create new sender account.
    // The account won't be empty because its nonce will be bumped.
    auto& sender_acc = (sender_ptr != nullptr) ? *sender_ptr : state.insert(tx.sender);

    const auto execution_gas_limit = get<int64_t>(validation_result);

    const auto base_fee = (rev >= EVMC_LONDON) ? block.base_fee : 0;
    assert(tx.max_gas_price >= base_fee);                   // Checked at the front.
    assert(tx.max_gas_price >= tx.max_priority_gas_price);  // Checked at the front.
    const auto priority_gas_price =
        std::min(tx.max_priority_gas_price, tx.max_gas_price - base_fee);
    const auto effective_gas_price = base_fee + priority_gas_price;

    assert(effective_gas_price <= tx.max_gas_price);
    const auto tx_max_cost = tx.gas_limit * effective_gas_price;

    sender_acc.balance -= tx_max_cost;  // Modify sender balance after all checks.

    int64_t blob_gas_used = 0;
    if (tx.type == Transaction::Type::blob)
    {
        const auto blob_gas_price = fake_exponential(1, block.excess_blob_gas, 3338477);
        blob_gas_used = GAS_PER_BLOB * static_cast<int64_t>(tx.blob_hashes.size());
        const auto blob_fee = blob_gas_used * blob_gas_price;

        assert(sender_acc.balance >= blob_fee);  // Checked at the front.
        sender_acc.balance -= blob_fee;
    }

    Host host{rev, vm, state, block, tx};

    sender_acc.access_status = EVMC_ACCESS_WARM;  // Tx sender is always warm.
    if (tx.to.has_value())
        host.access_account(*tx.to);
    for (const auto& [a, storage_keys] : tx.access_list)
    {
        host.access_account(a);  // TODO: Return account ref.
        auto& storage = state.get(a).storage;
        for (const auto& key : storage_keys)
            storage[key].access_status = EVMC_ACCESS_WARM;
    }
    // EIP-3651: Warm COINBASE.
    // This may create an empty coinbase account. The account cannot be created unconditionally
    // because this breaks old revisions.
    if (rev >= EVMC_SHANGHAI)
        host.access_account(block.coinbase);

    const auto result = host.call(build_message(tx, execution_gas_limit));

    auto gas_used = tx.gas_limit - result.gas_left;

    const auto max_refund_quotient = rev >= EVMC_LONDON ? 5 : 2;
    const auto refund_limit = gas_used / max_refund_quotient;
    const auto refund = std::min(result.gas_refund, refund_limit);
    gas_used -= refund;
    assert(gas_used > 0);

    state.get(tx.sender).balance += tx_max_cost - gas_used * effective_gas_price;
    state.touch(block.coinbase).balance += gas_used * priority_gas_price;

    // Apply destructs.
    std::erase_if(state.get_accounts(),
        [](const std::pair<const address, Account>& p) noexcept { return p.second.destructed; });

    // Cumulative gas used is unknown in this scope.
    TransactionReceipt receipt{
        tx.type, result.status_code, gas_used, blob_gas_used, {}, host.take_logs(), {}, {}};

    // Cannot put it into constructor call because logs are std::moved from host instance.
    receipt.logs_bloom_filter = compute_bloom_filter(receipt.logs);

    // Delete empty accounts after every transaction. This is strictly required until Byzantium
    // where intermediate state root hashes are part of the transaction receipt.
    // TODO: Consider limiting this only to Spurious Dragon.
    if (rev >= EVMC_SPURIOUS_DRAGON)
        delete_empty_accounts(state);

    // Set accounts and their storage access status to cold in the end of transition process
    for (auto& [addr, acc] : state.get_accounts())
    {
        acc.transient_storage.clear();
        acc.access_status = EVMC_ACCESS_COLD;
        for (auto& [key, val] : acc.storage)
        {
            val.access_status = EVMC_ACCESS_COLD;
            val.original = val.current;
        }
    }

    return receipt;
}

[[nodiscard]] bytes rlp_encode(const Log& log)
{
    return rlp::encode_tuple(log.addr, log.topics, log.data);
}

[[nodiscard]] bytes rlp_encode(const Transaction& tx)
{
    // TODO: Refactor this function. For all type of transactions most of the code is similar.
    if (tx.type == Transaction::Type::legacy)
    {
        // rlp [nonce, gas_price, gas_limit, to, value, data, v, r, s];
        return rlp::encode_tuple(tx.nonce, tx.max_gas_price, static_cast<uint64_t>(tx.gas_limit),
            tx.to.has_value() ? tx.to.value() : bytes_view(), tx.value, tx.data, tx.v, tx.r, tx.s);
    }
    else if (tx.type == Transaction::Type::access_list)
    {
        if (tx.v > 1)
            throw std::invalid_argument("`v` value for eip2930 transaction must be 0 or 1");
        // tx_type +
        // rlp [nonce, gas_price, gas_limit, to, value, data, access_list, v, r, s];
        return bytes{0x01} +  // Transaction type (eip2930 type == 1)
               rlp::encode_tuple(tx.chain_id, tx.nonce, tx.max_gas_price,
                   static_cast<uint64_t>(tx.gas_limit),
                   tx.to.has_value() ? tx.to.value() : bytes_view(), tx.value, tx.data,
                   tx.access_list, static_cast<bool>(tx.v), tx.r, tx.s);
    }
    else if (tx.type == Transaction::Type::eip1559)
    {
        if (tx.v > 1)
            throw std::invalid_argument("`v` value for eip1559 transaction must be 0 or 1");
        // tx_type +
        // rlp [chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas, gas_limit, to, value,
        // data, access_list, sig_parity, r, s];
        return bytes{0x02} +  // Transaction type (eip1559 type == 2)
               rlp::encode_tuple(tx.chain_id, tx.nonce, tx.max_priority_gas_price, tx.max_gas_price,
                   static_cast<uint64_t>(tx.gas_limit),
                   tx.to.has_value() ? tx.to.value() : bytes_view(), tx.value, tx.data,
                   tx.access_list, static_cast<bool>(tx.v), tx.r, tx.s);
    }
    else
    {
        if (tx.v > 1)
            throw std::invalid_argument("`v` value for blob transaction must be 0 or 1");
        if (!tx.to.has_value())  // Blob tx has to have `to` address
            throw std::invalid_argument("`to` value for blob transaction must not be null");
        // tx_type +
        // rlp [chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas, gas_limit, to, value,
        // data, access_list, max_fee_per_blob_gas, blob_versioned_hashes, sig_parity, r, s];
        return bytes{stdx::to_underlying(Transaction::Type::blob)} +
               rlp::encode_tuple(tx.chain_id, tx.nonce, tx.max_priority_gas_price, tx.max_gas_price,
                   static_cast<uint64_t>(tx.gas_limit), tx.to.value(), tx.value, tx.data,
                   tx.access_list, tx.max_blob_gas_price, tx.blob_hashes, static_cast<bool>(tx.v),
                   tx.r, tx.s);
    }
}

[[nodiscard]] bytes rlp_encode(const TransactionReceipt& receipt)
{
    if (receipt.post_state.has_value())
    {
        assert(receipt.type == Transaction::Type::legacy);

        return rlp::encode_tuple(receipt.post_state.value(),
            static_cast<uint64_t>(receipt.cumulative_gas_used),
            bytes_view(receipt.logs_bloom_filter), receipt.logs);
    }
    else
    {
        const auto prefix = receipt.type == Transaction::Type::legacy ?
                                bytes{} :
                                bytes{stdx::to_underlying(receipt.type)};

        return prefix + rlp::encode_tuple(receipt.status == EVMC_SUCCESS,
                            static_cast<uint64_t>(receipt.cumulative_gas_used),
                            bytes_view(receipt.logs_bloom_filter), receipt.logs);
    }
}

[[nodiscard]] bytes rlp_encode(const Withdrawal& withdrawal)
{
    return rlp::encode_tuple(withdrawal.index, withdrawal.validator_index, withdrawal.recipient,
        withdrawal.amount_in_gwei);
}

}  // namespace evmone::state
