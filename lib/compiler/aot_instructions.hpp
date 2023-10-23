// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <limits>

#include "eof.hpp"
#include "aot_intx.hpp"
#include "aot_instructions_traits.hpp"
#include "aot_instructions_xmacro.hpp"
#include <ethash/keccak.hpp>

namespace evmone
{

/// Represents the pointer to the stack top item
/// and allows retrieving stack items and manipulating the pointer.
class StackTop
{
    uint256* m_top;

public:
    StackTop(uint256* top) noexcept : m_top{top} {}

    /// Returns the reference to the stack item by index, where 0 means the top item
    /// and positive index values the items further down the stack.
    /// Using [-1] is also valid, but .push() should be used instead.
    [[nodiscard]] uint256& operator[](int index) noexcept { return m_top[-index]; }

    /// Returns the reference to the stack top item.
    [[nodiscard]] uint256& top() noexcept { return *m_top; }

    /// Returns the current top item and move the stack top pointer down.
    /// The value is returned by reference because the stack slot remains valid.
    [[nodiscard]] uint256& pop() noexcept { return *m_top--; }

    /// Assigns the value to the stack top and moves the stack top pointer up.
    void push(const uint256& value) noexcept { *++m_top = value; }
};

constexpr auto max_buffer_size = std::numeric_limits<uint32_t>::max();

/// The size of the EVM 256-bit word.
constexpr auto word_size = 32;

/// Returns number of words what would fit to provided number of bytes,
/// i.e. it rounds up the number bytes to number of words.
inline constexpr int64_t num_words(uint64_t size_in_bytes) noexcept
{
    return static_cast<int64_t>((size_in_bytes + (word_size - 1)) / word_size);
}

/// Computes gas cost of copying the given amount of bytes to/from EVM memory.
inline constexpr int64_t copy_cost(uint64_t size_in_bytes) noexcept
{
    constexpr auto WordCopyCost = 3;
    return num_words(size_in_bytes) * WordCopyCost;
}

/// Grows EVM memory and checks its cost.
///
/// This function should not be inlined because this may affect other inlining decisions:
/// - making check_memory() too costly to inline,
/// - making mload()/mstore()/mstore8() too costly to inline.
///
/// TODO: This function should be moved to Memory class.
[[gnu::noinline]] inline int64_t grow_memory(
    int64_t gas_left, Memory& memory, uint64_t new_size) noexcept
{
    // This implementation recomputes memory.size(). This value is already known to the caller
    // and can be passed as a parameter, but this make no difference to the performance.

    const auto new_words = num_words(new_size);
    const auto current_words = static_cast<int64_t>(memory.size() / word_size);
    const auto new_cost = 3 * new_words + new_words * new_words / 512;
    const auto current_cost = 3 * current_words + current_words * current_words / 512;
    const auto cost = new_cost - current_cost;

    gas_left -= cost;
    if (gas_left >= 0) [[likely]]
        memory.grow(static_cast<size_t>(new_words * word_size));
    return gas_left;
}

/// Check memory requirements for "copy" instructions.
inline bool check_memory(
    int64_t& gas_left, Memory& memory, const uint256& offset, const uint256& size) noexcept
{
    if (size == 0)  // Copy of size 0 is always valid (even if offset is huge).
        return true;

    if (offset > max_buffer_size)
        return false;

    const auto new_size = static_cast<uint64_t>(offset + size);
    if (new_size > memory.size())
        gas_left = grow_memory(gas_left, memory, new_size);

    return gas_left >= 0;  // Always true for no-grow case.
}

namespace instr::core
{

/// Unified interface for all OPCODE handlers
#define PARAMS StackTop& stack              [[maybe_unused]], \
               int64_t& gas_left            [[maybe_unused]], \
               evmc_status_code& status     [[maybe_unused]], \
               native_jumpdest& jump_addr   [[maybe_unused]], \
               ExecutionState& state        [[maybe_unused]]

/// Set the status code and return (assuming gas_left has been modified).
#define RETURN_STATUS(StatusCode) { status = StatusCode; return; }

/// The "core" instruction implementations.
///
/// These are minimal EVM instruction implementations which assume:
/// - the stack requirements (overflow, underflow) have already been checked,
/// - the "base" gas const has already been charged,
/// - the `stack` pointer points to the EVM stack top element.
/// Moreover, these implementations _do not_ inform about new stack height
/// after execution. The adjustment must be performed by the caller.
inline void unimplemented_op(PARAMS) noexcept { __builtin_unreachable(); }
inline void pop(PARAMS) noexcept { std::ignore = stack.pop(); }

template <evmc_status_code Status>
inline void stop_impl(PARAMS) noexcept { status = Status; }
inline constexpr auto stop = stop_impl<EVMC_SUCCESS>;
inline constexpr auto invalid = stop_impl<EVMC_INVALID_INSTRUCTION>;

inline void add(PARAMS) noexcept
{
    stack.top() += stack.pop();
}

inline void mul(PARAMS) noexcept
{
    stack.top() *= stack.pop();
}

inline void sub(PARAMS) noexcept
{
    stack[1] = stack[0] - stack[1];
    std::ignore = stack.pop();
}

inline void div(PARAMS) noexcept
{
    auto& v = stack[1];
    v = v != 0 ? stack[0] / v : 0;
    std::ignore = stack.pop();
}

inline void sdiv(PARAMS) noexcept
{
    auto& v = stack[1];
    v = v != 0 ? uint256(int256(stack[0]) / int256(v)) : 0;
    std::ignore = stack.pop();
}

inline void mod(PARAMS) noexcept
{
    auto& v = stack[1];
    v = v != 0 ? stack[0] % v : 0;
    std::ignore = stack.pop();
}

inline void smod(PARAMS) noexcept
{
    auto& v = stack[1];
    v = v != 0 ? uint256(int256(stack[0]) % int256(v)) : 0;
    std::ignore = stack.pop();
}

inline void addmod(PARAMS) noexcept
{
    const auto& x = stack.pop();
    const auto& y = stack.pop();
    auto& m = stack.top();
    m = m != 0 ? intx::addmod(x, y, m) : 0;
}

inline void mulmod(PARAMS) noexcept
{
    const auto& x = stack[0];
    const auto& y = stack[1];
    auto& m = stack[2];
    m = m != 0 ? intx::mulmod(x, y, m) : 0;
}

inline void exp(PARAMS) noexcept
{
    const auto& base = stack.pop();
    auto& exponent = stack.top();

    const auto exponent_significant_bytes =
        static_cast<int>(intx::count_significant_bytes(exponent));
    const auto exponent_cost = state.rev >= EVMC_SPURIOUS_DRAGON ? 50 : 10;
    const auto additional_cost = exponent_significant_bytes * exponent_cost;
    if ((gas_left -= additional_cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    exponent = intx::exp(base, exponent);
}

inline void signextend(PARAMS) noexcept
{
    const auto& ext = stack.pop();
    auto& x = stack.top();

    if (ext < 31)  // For 31 we also don't need to do anything.
    {
        // https://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend
        auto b = (uint(ext) + 1) << 3;
        const int256 mask = 1u << (b - 1);
        x = uint256((int256(x) ^ mask) - mask);
    }
}

inline void lt(PARAMS) noexcept
{
    const auto& x = stack.pop();
    stack[0] = x < stack[0] ? 1 : 0;
}

inline void gt(PARAMS) noexcept
{
    const auto& x = stack.pop();
    stack[0] = stack[0] < x ? 1 : 0;  // Arguments are swapped and < is used.
}

inline void slt(PARAMS) noexcept
{
    const auto& x = stack.pop();
    stack[0] = int256(x) < int256(stack[0]) ? 1 : 0;
}

inline void sgt(PARAMS) noexcept
{
    const auto& x = stack.pop();
    stack[0] = int256(stack[0]) < int256(x) ? 1 : 0;
}

inline void eq(PARAMS) noexcept
{
    stack[1] = stack[0] == stack[1] ? 1 : 0;
    std::ignore = stack.pop();
}

inline void iszero(PARAMS) noexcept
{
    stack.top() = stack.top() == 0 ? 1 : 0;
}

inline void and_(PARAMS) noexcept
{
    stack.top() &= stack.pop();
}

inline void or_(PARAMS) noexcept
{
    stack.top() |= stack.pop();
}

inline void xor_(PARAMS) noexcept
{
    stack.top() ^= stack.pop();
}

inline void not_(PARAMS) noexcept
{
    stack.top() = ~stack.top();
}

inline void byte(PARAMS) noexcept
{
    const auto& n = stack.pop();
    auto& x = stack.top();

    const bool n_valid = n < 32;
    const uint8_t byte_mask = (n_valid ? 0xff : 0);
    const uint8_t index = 31 - uint8_t(n % 32);
    x = (x >> index) & byte_mask;
}

inline void shl(PARAMS) noexcept
{
    stack.top() <<= stack.pop();
}

inline void shr(PARAMS) noexcept
{
    stack.top() >>= stack.pop();
}

inline void sar(PARAMS) noexcept
{
    const auto& y = stack.pop();
    auto& x = stack.top();

    const bool is_neg = x < 0;
    const auto sign_mask = is_neg ? ~uint256{} : uint256{};

    const auto mask_shift = (y < 256) ? (256 - y) : 0;
    x = (x >> y) | (sign_mask << mask_shift);
}

inline void keccak256(PARAMS) noexcept
{
    const auto& index = stack.pop();
    auto& size = stack.top();

    if (!check_memory(gas_left, state.memory, index, size))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    const auto i = static_cast<size_t>(index);
    const auto s = static_cast<size_t>(size);
    const auto w = num_words(s);
    const auto cost = w * 6;
    if ((gas_left -= cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    auto data = s != 0 ? &state.memory[i] : nullptr;
    size = intx::load_be256(ethash::keccak256(data, s));
}


inline void address(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.msg->recipient));
}

inline void balance(PARAMS) noexcept
{
    auto& x = stack.top();
    const auto addr = intx::trunc_be<evmc::address>(x);

    if (state.rev >= EVMC_BERLIN && state.host.access_account(addr) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::additional_cold_account_access_cost) < 0)
            RETURN_STATUS(EVMC_OUT_OF_GAS)
    }

    x = intx::load_be256(state.host.get_balance(addr));
}

inline void origin(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.get_tx_context().tx_origin));
}

inline void caller(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.msg->sender));
}

inline void callvalue(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.msg->value));
}

inline void calldataload(PARAMS) noexcept
{
    auto& index = stack.top();

    if (state.msg->input_size < index)
        index = 0;
    else
    {
        const auto begin = static_cast<size_t>(index);
        const auto end = std::min(begin + 32, state.msg->input_size);

        uint8_t data[32] = {};
        for (size_t i = 0; i < (end - begin); ++i)
            data[i] = state.msg->input_data[begin + i];

        index = intx::load_be256(data);
    }
}

inline void calldatasize(PARAMS) noexcept
{
    stack.push(state.msg->input_size);
}

inline void calldatacopy(PARAMS) noexcept
{
    const auto& mem_index = stack.pop();
    const auto& input_index = stack.pop();
    const auto& size = stack.pop();

    if (!check_memory(gas_left, state.memory, mem_index, size))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    auto dst = static_cast<size_t>(mem_index);
    auto src = state.msg->input_size < input_index ? state.msg->input_size :
                                                     static_cast<size_t>(input_index);
    auto s = static_cast<size_t>(size);
    auto copy_size = std::min(s, state.msg->input_size - src);

    if (const auto cost = copy_cost(s); (gas_left -= cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    if (copy_size > 0)
        std::memcpy(&state.memory[dst], &state.msg->input_data[src], copy_size);

    if (s - copy_size > 0)
        std::memset(&state.memory[dst + copy_size], 0, s - copy_size);
}

inline void codesize(PARAMS) noexcept
{
    stack.push(state.original_code.size());
}

inline void codecopy(PARAMS) noexcept
{
    // TODO: Similar to calldatacopy().

    const auto& mem_index = stack.pop();
    const auto& input_index = stack.pop();
    const auto& size = stack.pop();

    if (!check_memory(gas_left, state.memory, mem_index, size))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    const auto code_size = state.original_code.size();
    const auto dst = static_cast<size_t>(mem_index);
    const auto src = code_size < input_index ? code_size : static_cast<size_t>(input_index);
    const auto s = static_cast<size_t>(size);
    const auto copy_size = std::min(s, code_size - src);

    if (const auto cost = copy_cost(s); (gas_left -= cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    // TODO: Add unit tests for each combination of conditions.
    if (copy_size > 0)
        std::memcpy(&state.memory[dst], &state.original_code[src], copy_size);

    if (s - copy_size > 0)
        std::memset(&state.memory[dst + copy_size], 0, s - copy_size);
}


inline void gasprice(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.get_tx_context().tx_gas_price));
}

inline void basefee(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.get_tx_context().block_base_fee));
}

inline void blobhash(PARAMS) noexcept
{
    auto& index = stack.top();
    const auto& tx = state.get_tx_context();

    index = (index < tx.blob_hashes_count) ?
                intx::load_be256(tx.blob_hashes[static_cast<size_t>(index)]) :
                0;
}

inline void extcodesize(PARAMS) noexcept
{
    auto& x = stack.top();
    const auto addr = intx::trunc_be<evmc::address>(x);

    if (state.rev >= EVMC_BERLIN && state.host.access_account(addr) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::additional_cold_account_access_cost) < 0)
            RETURN_STATUS(EVMC_OUT_OF_GAS)
    }

    x = state.host.get_code_size(addr);
}

inline void extcodecopy(PARAMS) noexcept
{
    const auto addr = intx::trunc_be<evmc::address>(stack.pop());
    const auto& mem_index = stack.pop();
    const auto& input_index = stack.pop();
    const auto& size = stack.pop();

    if (!check_memory(gas_left, state.memory, mem_index, size)) 
            RETURN_STATUS(EVMC_OUT_OF_GAS)

    const auto s = static_cast<size_t>(size);
    if (const auto cost = copy_cost(s); (gas_left -= cost) < 0) 
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    if (state.rev >= EVMC_BERLIN && state.host.access_account(addr) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::additional_cold_account_access_cost) < 0) 
            RETURN_STATUS(EVMC_OUT_OF_GAS)
    }

    if (s > 0)
    {
        const auto src =
            (max_buffer_size < input_index) ? max_buffer_size : static_cast<size_t>(input_index);
        const auto dst = static_cast<size_t>(mem_index);
        const auto num_bytes_copied = state.host.copy_code(addr, src, &state.memory[dst], s);
        if (const auto num_bytes_to_clear = s - num_bytes_copied; num_bytes_to_clear > 0)
            std::memset(&state.memory[dst + num_bytes_copied], 0, num_bytes_to_clear);
    }
}

inline void returndatasize(PARAMS) noexcept
{
    stack.push(state.return_data.size());
}

inline void returndatacopy(PARAMS) noexcept
{
    const auto& mem_index = stack.pop();
    const auto& input_index = stack.pop();
    const auto& size = stack.pop();

    if (!check_memory(gas_left, state.memory, mem_index, size))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    auto dst = static_cast<size_t>(mem_index);
    auto s = static_cast<size_t>(size);

    if (state.return_data.size() < input_index)
        RETURN_STATUS(EVMC_INVALID_MEMORY_ACCESS)
    auto src = static_cast<size_t>(input_index);

    if (src + s > state.return_data.size())
        RETURN_STATUS(EVMC_INVALID_MEMORY_ACCESS)

    if (const auto cost = copy_cost(s); (gas_left -= cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    if (s > 0)
        std::memcpy(&state.memory[dst], &state.return_data[src], s);
}

inline void extcodehash(PARAMS) noexcept
{
    auto& x = stack.top();
    const auto addr = intx::trunc_be<evmc::address>(x);

    if (state.rev >= EVMC_BERLIN && state.host.access_account(addr) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::additional_cold_account_access_cost) < 0)
            RETURN_STATUS(EVMC_OUT_OF_GAS)
    }

    x = intx::load_be256(state.host.get_code_hash(addr));
}

inline void blockhash(PARAMS) noexcept
{
    auto& number = stack.top();

    const auto upper_bound = state.get_tx_context().block_number;
    const auto lower_bound = std::max(upper_bound - 256, decltype(upper_bound){0});
    const auto n = static_cast<int64_t>(number);
    const auto header =
        (number < uint256(upper_bound) && n >= lower_bound) ? state.host.get_block_hash(n) : evmc::bytes32{};
    number = intx::load_be256(header);
}

inline void coinbase(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.get_tx_context().block_coinbase));
}

inline void timestamp(PARAMS) noexcept
{
    // TODO: Add tests for negative timestamp?
    stack.push(static_cast<uint64_t>(state.get_tx_context().block_timestamp));
}

inline void number(PARAMS) noexcept
{
    // TODO: Add tests for negative block number?
    stack.push(static_cast<uint64_t>(state.get_tx_context().block_number));
}

inline void prevrandao(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.get_tx_context().block_prev_randao));
}

inline void gaslimit(PARAMS) noexcept
{
    stack.push(static_cast<uint64_t>(state.get_tx_context().block_gas_limit));
}

inline void chainid(PARAMS) noexcept
{
    stack.push(intx::load_be256(state.get_tx_context().chain_id));
}

inline void selfbalance(PARAMS) noexcept
{
    // TODO: introduce selfbalance in EVMC?
    stack.push(intx::load_be256(state.host.get_balance(state.msg->recipient)));
}

inline void mload(PARAMS) noexcept
{
    auto& index = stack.top();

    if (!check_memory(gas_left, state.memory, index, 32))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    index = intx::load_be256_unsafe(&state.memory[static_cast<size_t>(index)]);
}

inline void mstore(PARAMS) noexcept
{
    const auto& index = stack.pop();
    const auto& value = stack.pop();

    if (!check_memory(gas_left, state.memory, index, 32))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    intx::store_be256_unsafe(&state.memory[static_cast<size_t>(index)], value);
}

inline void mstore8(PARAMS) noexcept
{
    const auto& index = stack.pop();
    const auto& value = stack.pop();

    if (!check_memory(gas_left, state.memory, index, 1))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    state.memory[static_cast<size_t>(index)] = static_cast<uint8_t>(value);
}

EVMC_EXPORT void sload(PARAMS) noexcept;

EVMC_EXPORT void sstore(PARAMS) noexcept;

inline constexpr auto rjump = unimplemented_op;
inline constexpr auto rjumpi = unimplemented_op;
inline constexpr auto rjumpv = unimplemented_op;

/// Internal jump implementation for JUMP/JUMPI instructions.
inline native_jumpdest jump_impl(ExecutionState& state, const uint256& offset, const JumpdestMap& jumpdest_map) noexcept
{
    const auto dst = jumpdest_map.get_jumpdest(offset);
    if (!dst) {
        state.status = EVMC_BAD_JUMP_DESTINATION;
        return state.bad_jump_handler;
    }
    return dst;
}

inline void jump(PARAMS, const JumpdestMap& jumpdest_map) noexcept
{
    jump_addr = jump_impl(state, stack.pop(), jumpdest_map);
}

inline void jumpi(PARAMS, const JumpdestMap& jumpdest_map) noexcept
{
    const auto& dst = stack.pop();
    const auto& cond = stack.pop();
    jump_addr = cond ? jump_impl(state, dst, jumpdest_map) : nullptr;
}

inline void jumpdest(PARAMS) noexcept
{
    // Reset the jump address previously set by JUMP/JUMPI.
    jump_addr = nullptr;
}

inline void pc(PARAMS, const uint256& counter) noexcept
{
    stack.push(counter);
}

inline void msize(PARAMS) noexcept
{
    stack.push(state.memory.size());
}

inline void gas(PARAMS) noexcept
{
    stack.push(uint256(gas_left));
}

inline void tload(PARAMS) noexcept
{
    auto& x = stack.top();
    const auto key = intx::store_be256<evmc::bytes32>(x);
    const auto value = state.host.get_transient_storage(state.msg->recipient, key);
    x = intx::load_be256(value);
}

inline void tstore(PARAMS) noexcept
{
    if (state.in_static_mode()) {
        gas_left = 0;
        RETURN_STATUS(EVMC_STATIC_MODE_VIOLATION)
    }

    const auto key = intx::store_be256<evmc::bytes32>(stack.pop());
    const auto value = intx::store_be256<evmc::bytes32>(stack.pop());
    state.host.set_transient_storage(state.msg->recipient, key, value);
}

inline void push0(PARAMS) noexcept
{
    stack.push(0);
}

inline void push(PARAMS, const uint256& value) noexcept
{
    stack.push(value);
}

/// DUP instruction implementation.
/// @tparam N  The number as in the instruction definition, e.g. DUP3 is dup<3>.
template <int N>
inline void dup(PARAMS) noexcept
{
    static_assert(N >= 1 && N <= 16);
    stack.push(stack[N - 1]);
}

/// SWAP instruction implementation.
/// @tparam N  The number as in the instruction definition, e.g. SWAP3 is swap<3>.
template <int N>
inline void swap(PARAMS) noexcept
{
    static_assert(N >= 1 && N <= 16);
    std::swap(stack.top(), stack[N]);
}

inline void dupn(PARAMS, const uint16_t& imm) noexcept
{
    const auto n = imm + 1;

    const auto stack_size = &stack.top() - state.stack_space.bottom();

    if (stack_size < n)
        RETURN_STATUS(EVMC_STACK_UNDERFLOW)

    stack.push(stack[n - 1]);
}

inline void swapn(PARAMS, const uint16_t& imm) noexcept
{
    const auto n = imm + 1;

    const auto stack_size = &stack.top() - state.stack_space.bottom();

    if (stack_size <= n)
        RETURN_STATUS(EVMC_STACK_UNDERFLOW)

    // TODO: This may not be optimal, see instr::core::swap().
    std::swap(stack.top(), stack[n]);
}

inline void mcopy(PARAMS) noexcept
{
    const auto& dst_u256 = stack.pop();
    const auto& src_u256 = stack.pop();
    const auto& size_u256 = stack.pop();

    if (!check_memory(gas_left, state.memory, std::max(dst_u256, src_u256), size_u256))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    const auto dst = static_cast<size_t>(dst_u256);
    const auto src = static_cast<size_t>(src_u256);
    const auto size = static_cast<size_t>(size_u256);

    if (const auto cost = copy_cost(size); (gas_left -= cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    if (size > 0)
        std::memmove(&state.memory[dst], &state.memory[src], size);
}

inline void dataload(PARAMS) noexcept
{
    auto& index = stack.top();

    if (state.data.size() < 32 || (state.data.size() - 32) < index)
        RETURN_STATUS(EVMC_INVALID_MEMORY_ACCESS)  // TODO: Introduce EVMC_INVALID_DATA_ACCESS

    const auto begin = static_cast<size_t>(index);
    index = intx::load_be256_unsafe(&state.data[begin]);
}

inline void datasize(PARAMS) noexcept
{
    stack.push(state.data.size());
}

inline void dataloadn(PARAMS, const uint16_t index) noexcept
{
    stack.push(intx::load_be256_unsafe(&state.data[index]));
}

inline void datacopy(PARAMS) noexcept
{
    const auto& mem_index = stack.pop();
    const auto& data_index = stack.pop();
    const auto& size = stack.pop();

    if (!check_memory(gas_left, state.memory, mem_index, size))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    const auto s = static_cast<size_t>(size);

    if (state.data.size() < s || state.data.size() - s < data_index)
        RETURN_STATUS(EVMC_INVALID_MEMORY_ACCESS)  // TODO: Introduce EVMC_INVALID_DATA_ACCESS

    if (const auto cost = copy_cost(s); (gas_left -= cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    if (s > 0)
    {
        const auto src = static_cast<size_t>(data_index);
        const auto dst = static_cast<size_t>(mem_index);
        std::memcpy(&state.memory[dst], &state.data[src], s);
    }
}

template <size_t NumTopics>
inline void log(PARAMS) noexcept
{
    static_assert(NumTopics <= 4);

    if (state.in_static_mode()) {
        gas_left = 0;
        RETURN_STATUS(EVMC_STATIC_MODE_VIOLATION)
    }

    const auto& offset = stack.pop();
    const auto& size = stack.pop();

    if (!check_memory(gas_left, state.memory, offset, size))
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    const auto o = static_cast<size_t>(offset);
    const auto s = static_cast<size_t>(size);

    const auto cost = int64_t(s) * 8;
    if ((gas_left -= cost) < 0)
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    std::array<evmc::bytes32, NumTopics> topics;  // NOLINT(cppcoreguidelines-pro-type-member-init)
    for (auto& topic : topics)
        topic = intx::store_be256<evmc::bytes32>(stack.pop());

    const auto data = s != 0 ? &state.memory[o] : nullptr;
    state.host.emit_log(state.msg->recipient, data, s, topics.data(), NumTopics);
}


template <Opcode Op>
EVMC_EXPORT void call_impl(PARAMS) noexcept;
inline constexpr auto call = call_impl<OP_CALL>;
inline constexpr auto callcode = call_impl<OP_CALLCODE>;
inline constexpr auto delegatecall = call_impl<OP_DELEGATECALL>;
inline constexpr auto staticcall = call_impl<OP_STATICCALL>;

template <Opcode Op>
EVMC_EXPORT void create_impl(PARAMS) noexcept;
inline constexpr auto create = create_impl<OP_CREATE>;
inline constexpr auto create2 = create_impl<OP_CREATE2>;
inline constexpr auto callf = unimplemented_op;
inline constexpr auto retf = unimplemented_op;

template <evmc_status_code StatusCode>
inline void return_impl(PARAMS) noexcept
{
    const auto& offset = stack[0];
    const auto& size = stack[1];

    if (!check_memory(gas_left, state.memory, offset, size)) 
        RETURN_STATUS(EVMC_OUT_OF_GAS)

    state.output_size = static_cast<size_t>(size);
    if (state.output_size != 0)
        state.output_offset = static_cast<size_t>(offset);
    status = StatusCode;
}
inline constexpr auto return_ = return_impl<EVMC_SUCCESS>;
inline constexpr auto revert = return_impl<EVMC_REVERT>;

inline void selfdestruct(PARAMS) noexcept
{
    if (state.in_static_mode()) {
        status = EVMC_STATIC_MODE_VIOLATION;
        return;
    }

    const auto beneficiary = intx::trunc_be<evmc::address>(stack[0]);

    if (state.rev >= EVMC_BERLIN && state.host.access_account(beneficiary) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::cold_account_access_cost) < 0) {
            status = EVMC_OUT_OF_GAS;
            return;
        }
    }

    if (state.rev >= EVMC_TANGERINE_WHISTLE)
    {
        if (state.rev == EVMC_TANGERINE_WHISTLE || state.host.get_balance(state.msg->recipient))
        {
            // After TANGERINE_WHISTLE apply additional cost of
            // sending value to a non-existing account.
            if (!state.host.account_exists(beneficiary))
            {
                if ((gas_left -= 25000) < 0) {
                    status = EVMC_OUT_OF_GAS;
                    return;
                }
            }
        }
    }

    if (state.host.selfdestruct(state.msg->recipient, beneficiary))
    {
        if (state.rev < EVMC_LONDON)
            state.gas_refund += 24000;
    }
}


/// Maps an opcode to the instruction implementation.
///
/// The set of template specializations which map opcodes `Op` to the function
/// implementing the instruction identified by the opcode.
///     instr::impl<OP_DUP1>(/*...*/);
/// The unspecialized template is invalid and should never to used.
template <Opcode Op>
inline constexpr auto impl = nullptr;

#undef ON_OPCODE_IDENTIFIER
#define ON_OPCODE_IDENTIFIER(OPCODE, IDENTIFIER) \
    template <>                                  \
    inline constexpr auto impl<OPCODE> = IDENTIFIER;  // opcode -> implementation
MAP_OPCODES
#undef ON_OPCODE_IDENTIFIER
#define ON_OPCODE_IDENTIFIER ON_OPCODE_IDENTIFIER_DEFAULT
}  // namespace instr::core
}  // namespace evmone
