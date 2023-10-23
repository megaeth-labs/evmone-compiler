// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <bit>
#include "aot_execution_state.hpp"

namespace evmone::intx
{

template <typename T>
[[noreturn]] inline void throw_(const char* what)
{
#if __cpp_exceptions
    throw T{what};
#else
    std::fputs(what, stderr);
    std::abort();
#endif
}

inline constexpr int from_dec_digit(char c)
{
    if (c < '0' || c > '9')
        throw_<std::invalid_argument>("invalid digit");
    return c - '0';
}

inline constexpr int from_hex_digit(char c)
{
    if (c >= 'a' && c <= 'f')
        return c - ('a' - 10);
    if (c >= 'A' && c <= 'F')
        return c - ('A' - 10);
    return from_dec_digit(c);
}

template <typename Int>
inline constexpr Int from_string(const char* str)
{
    auto s = str;
    auto x = Int{};
    int num_digits = 0;

    if (s[0] == '0' && s[1] == 'x')
    {
        s += 2;
        while (const auto c = *s++)
        {
            if (++num_digits > int{sizeof(x) * 2})
                throw_<std::out_of_range>(str);
            x = (x << uint64_t{4}) | uint(from_hex_digit(c));
        }
        return x;
    }

    while (const auto c = *s++)
    {
        if (num_digits++ > std::numeric_limits<Int>::digits10)
            throw_<std::out_of_range>(str);

        const auto d = uint(from_dec_digit(c));
        x = x * Int{10} + d;
        if (x < d)
            throw_<std::out_of_range>(str);
    }
    return x;
}

inline consteval uint256 operator"" _u256(const char* s)
{
    return from_string<uint256>(s);
}

/// Convert native representation to/from big-endian byte order.
inline uint256 to_big_endian(const uint256& x) noexcept
{
    if constexpr (std::endian::native == std::endian::little) {
        uint256 r;
        const auto x_bytes = reinterpret_cast<const uint64_t*>(&x);
        auto r_bytes = reinterpret_cast<uint64_t*>(&r);
        for (size_t i = 0; i < 4; ++i)
            r_bytes[3 - i] = std::byteswap(x_bytes[i]);
        return r;
    } else {
        return x;
    }
}

/// Loads a uint256 integer value from bytes of big-endian order.
/// If the size of bytes is smaller than the result, the value is zero-extended.
template <unsigned M>
inline uint256 load_be256(const uint8_t (&src)[M]) noexcept
{
    static_assert(M <= 32);
    uint256 x{};
    auto bytes = reinterpret_cast<uint8_t*>(&x);
    std::memcpy(&bytes[32 - M], src, M);
    return to_big_endian(x);
}

/// Loads a uint256 value from the .bytes field of an object of type T.
template <typename T>
inline uint256 load_be256(const T& t) noexcept
{
    return load_be256(t.bytes);
}

/// Loads a uint256 value from a buffer. The user must make sure
/// that the provided buffer is big enough. Therefore marked "unsafe".
inline uint256 load_be256_unsafe(const uint8_t* src) noexcept
{
    return to_big_endian(*reinterpret_cast<const uint256*>(src));
}

/// Stores a uint256 value at the provided pointer in big-endian order. The user must make sure
/// that the provided buffer is big enough to fit the value. Therefore marked "unsafe".
inline void store_be256_unsafe(uint8_t* dst, const uint256& x) noexcept
{
    const auto d = to_big_endian(x);
    *reinterpret_cast<uint256*>(dst) = d;
}

/// Stores a uint256 value in .bytes field of type DstT. The .bytes must match
/// the size of uint256.
template <typename DstT>
inline evmc::bytes32 store_be256(const uint256& x) noexcept
{
    DstT r{};
    store_be256_unsafe(r.bytes, x);
    return r;
}

/// Stores the truncated value of an uint in a bytes array.
/// Only the least significant bytes from big-endian representation of the uint
/// are stored in the result bytes array up to array's size.
template <unsigned M>
inline void trunc_be(uint8_t (&dst)[M], const uint256& x) noexcept
{
    static_assert(M < 32, "destination must be smaller than the source value");
    const auto d = to_big_endian(x);
    auto bytes = reinterpret_cast<const uint8_t*>(&d);
    std::memcpy(dst, &bytes[sizeof(d) - M], M);
}

/// Stores the truncated value of a uint256 in the .bytes field of an object of type T.
template <typename T>
inline T trunc_be(const uint256& x) noexcept
{
    T r{};
    trunc_be(r.bytes, x);
    return r;
}

inline uint256 addmod(const uint256& x, const uint256& y, const uint256& mod) noexcept
{
    // FIXME: implement the fast path of intx::addmod?
    return (uint257(x) + y) % mod;
}

inline uint256 mulmod(const uint256& x, const uint256& y, const uint256& mod) noexcept
{
    return (uint512(x) * y) % mod;
}

inline constexpr uint256 exp(uint256 base, uint256 exponent) noexcept
{
    auto result = uint256{1};
    if (base == 2)
        return result << exponent;

    while (exponent != 0)
    {
        if ((exponent & 1) != 0)
            result *= base;
        base *= base;
        exponent >>= 1;
    }
    return result;
}

/// Returns the number of leading zero bits in a uint128 value.
inline constexpr int clz128(uint128 x) noexcept
{
    uint64_t hi = x >> 64;
    uint64_t lo = uint64_t(x);
    return hi == 0 ? std::countl_zero(lo) + 64 : std::countl_zero(hi);
}

/// Returns the number of leading zero bits in a uint256 value.
inline constexpr int clz256(uint256 x) noexcept
{
    uint128 hi = x >> 128;
    uint128 lo = uint128(x);
    return hi == 0 ? clz128(lo) + 128 : clz128(hi);
}

/// Returns the number of significant bytes in a uint256 value.
inline constexpr unsigned count_significant_bytes(uint256 x) noexcept
{
    return (256 - unsigned(clz256(x)) + 7) / 8;
}

} // namespace evmone::intx
