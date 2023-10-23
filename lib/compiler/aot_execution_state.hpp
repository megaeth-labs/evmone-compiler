// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace evmone
{

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbit-int-extension"
using uint128 = unsigned _BitInt(128);
using uint256 = unsigned _BitInt(256);
using uint257 = unsigned _BitInt(257);
using uint512 = unsigned _BitInt(512);
using int256 = signed _BitInt(256);
#pragma GCC diagnostic pop

using bytes = std::basic_string<uint8_t>;
using bytes_view = std::basic_string_view<uint8_t>;

/// Jump destination in the native code.
using native_jumpdest = void*;

/// Map of valid jump destinations.
struct JumpdestMap
{
    /// The maximum number of jump destinations in a contract.
    static constexpr auto limit = 1 << 10;

    /// Actual number of jump destinations.
    const size_t size;

    /// Bytecode offsets in ascending order.
    std::array<uint256, limit> keys;

    /// Jump destinations in the compiled contract.
    std::array<native_jumpdest, limit> vals;

    /// Returns if the target bytecode offset is a legit jump destination.
    /// Only used at compile-time.
    [[nodiscard]] consteval bool is_jumpdest(const uint256& offset) const noexcept
    {
        return std::binary_search(keys.begin(), keys.begin() + size, offset);
    }

    /// Returns the corresponding jump destination of a bytecode offset.
    [[nodiscard]] native_jumpdest get_jumpdest(const uint256& offset) const noexcept
    {
        auto lower = std::lower_bound(keys.begin(), keys.begin() + size, offset);
        auto index = size_t(lower - keys.begin());
        return index < size ? vals[index] : native_jumpdest{};
    }
};

/// Provides memory for EVM stack.
class StackSpace
{
public:
    /// The maximum number of EVM stack items.
    static constexpr auto limit = 1024;

    /// Returns the pointer to the "bottom", i.e. below the stack space.
    [[nodiscard, clang::no_sanitize("bounds")]] uint256* bottom() noexcept
    {
        return m_stack_space - 1;
    }

private:
    /// The storage allocated for maximum possible number of items.
    /// Items are aligned to 256 bits for better packing in cache lines.
    alignas(sizeof(uint256)) uint256 m_stack_space[limit];
};


/// The EVM memory.
///
/// The implementations uses initial allocation of 4k and then grows capacity with 2x factor.
/// Some benchmarks has been done to confirm 4k is ok-ish value.
class Memory
{
    /// The size of allocation "page".
    static constexpr size_t page_size = 4 * 1024;

    /// Pointer to allocated memory.
    uint8_t* m_data = nullptr;

    /// The "virtual" size of the memory.
    size_t m_size = 0;

    /// The size of allocated memory. The initialization value is the initial capacity.
    size_t m_capacity = page_size;

    [[noreturn, gnu::cold]] static void handle_out_of_memory() noexcept { std::terminate(); }

    void allocate_capacity() noexcept
    {
        m_data = static_cast<uint8_t*>(std::realloc(m_data, m_capacity));
        if (m_data == nullptr)
            handle_out_of_memory();
    }

public:
    /// Creates Memory object with initial capacity allocation.
    Memory() noexcept { allocate_capacity(); }

    /// Frees all allocated memory.
    ~Memory() noexcept { std::free(m_data); }

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    uint8_t& operator[](size_t index) noexcept { return m_data[index]; }

    [[nodiscard]] const uint8_t* data() const noexcept { return m_data; }
    [[nodiscard]] size_t size() const noexcept { return m_size; }

    /// Grows the memory to the given size. The extend is filled with zeros.
    ///
    /// @param new_size  New memory size. Must be larger than the current size and multiple of 32.
    void grow(size_t new_size) noexcept
    {
        // Restriction for future changes. EVM always has memory size as multiple of 32 bytes.
        assert(new_size % 32 == 0);

        // Allow only growing memory. Include hint for optimizing compiler.
        assert(new_size > m_size);

        if (new_size > m_capacity)
        {
            m_capacity *= 2;  // Double the capacity.

            if (m_capacity < new_size)  // If not enough.
            {
                // Set capacity to required size rounded to multiple of page_size.
                m_capacity = ((new_size + (page_size - 1)) / page_size) * page_size;
            }

            allocate_capacity();
        }
        std::memset(m_data + m_size, 0, new_size - m_size);
        m_size = new_size;
    }

    /// Virtually clears the memory by setting its size to 0. The capacity stays unchanged.
    void clear() noexcept { m_size = 0; }
};


/// Generic execution state for generic instructions implementations.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
class ExecutionState
{
public:
    int64_t gas_refund = 0;
    Memory memory;
    const evmc_message* msg = nullptr;
    evmc::HostContext host;
    evmc_revision rev = {};
    bytes return_data;

    /// Address of the subroutine that handles invalid jump destinations.
    native_jumpdest bad_jump_handler {};

    /// Reference to original EVM code container.
    /// For legacy code this is a reference to entire original code.
    /// For EOF-formatted code this is a reference to entire container.
    bytes_view original_code;

    /// Reference to the EOF data section. May be empty.
    bytes_view data;

    evmc_status_code status = EVMC_SUCCESS;
    size_t output_offset = 0;
    size_t output_size = 0;

private:
    evmc_tx_context m_tx = {};

public:
    std::vector<const uint8_t*> call_stack;

    /// Stack space allocation.
    ///
    /// This is the last field to make other fields' keys of reasonable values.
    StackSpace stack_space;

    ExecutionState() noexcept = default;

    ExecutionState(const evmc_message& message, evmc_revision revision,
        const evmc_host_interface& host_interface, evmc_host_context* host_ctx, bytes_view _code,
        bytes_view _data) noexcept
      : msg{&message},
        host{host_interface, host_ctx},
        rev{revision},
        original_code{_code},
        data{_data}
    {}

    /// Resets the contents of the ExecutionState so that it could be reused.
    void reset(const evmc_message& message, evmc_revision revision,
        const evmc_host_interface& host_interface, evmc_host_context* host_ctx, bytes_view _code,
        bytes_view _data) noexcept
    {
        gas_refund = 0;
        memory.clear();
        msg = &message;
        host = {host_interface, host_ctx};
        rev = revision;
        return_data.clear();
        original_code = _code;
        data = _data;
        status = EVMC_SUCCESS;
        output_offset = 0;
        output_size = 0;
        m_tx = {};
    }

    [[nodiscard]] bool in_static_mode() const { return (msg->flags & EVMC_STATIC) != 0; }

    const evmc_tx_context& get_tx_context() noexcept
    {
        if (m_tx.block_timestamp == 0) [[unlikely]]
            m_tx = host.get_tx_context();
        return m_tx;
    }
};
}  // namespace evmone
