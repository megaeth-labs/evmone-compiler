// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "intx/intx.hpp"
#include "span"
#include "vector"

namespace evmmax
{
using uint256 = intx::uint256;
using namespace intx;
using namespace std::literals;


template <typename ArithT, typename ModCoeffsT>
struct PolyExtFieldElem
{
    static constexpr auto degree = ModCoeffsT::DEGREE;
    static constexpr auto arith = ArithT();

    std::vector<uint256> coeffs;

    explicit PolyExtFieldElem() { coeffs.resize(degree); }

    explicit PolyExtFieldElem(std::vector<uint256>&& _coeffs) : coeffs(_coeffs)
    {
        assert(coeffs.size() == degree);
    }

    static inline constexpr PolyExtFieldElem add(
        const PolyExtFieldElem& x, const PolyExtFieldElem& y)
    {
        PolyExtFieldElem result;

        for (size_t i = 0; i < degree; ++i)
            result.coeffs[i] = x.arith.add(x.coeffs[i], y.coeffs[i]);

        return result;
    }

    static inline constexpr PolyExtFieldElem sub(
        const PolyExtFieldElem& x, const PolyExtFieldElem& y)
    {
        PolyExtFieldElem result;

        for (size_t i = 0; i < degree; ++i)
            result.coeffs[i] = arith.sub(x.coeffs[i], y.coeffs[i]);

        return result;
    }

    static inline constexpr PolyExtFieldElem mul(const PolyExtFieldElem& x, const uint256& c)
    {
        PolyExtFieldElem result;

        for (size_t i = 0; i < degree; ++i)
            result.coeffs[i] = arith.mul(x.coeffs[i], c);

        return result;
    }

    static inline PolyExtFieldElem mul(
        const PolyExtFieldElem& x, const PolyExtFieldElem& y)
    {
        std::vector<uint256> b(2 * degree - 1);

        // Multiply
        for (size_t i = 0; i < degree; ++i)
        {
            for (size_t j = 0; j < degree; ++j)
                b[i + j] = arith.add(b[i + j], arith.mul(x.coeffs[i], y.coeffs[j]));
        }

        // Reduce by irreducible polynomial (extending polynomial)
        while (b.size() > degree)
        {
            auto top = b.back();
            auto exp = b.size() - degree - 1;
            b.pop_back();
            for (size_t i = 0; i < degree; ++i)
                b[i + exp] = arith.sub(b[i + exp], arith.mul(top, ModCoeffsT::MODULUS_COEFFS[i]));
        }

        return PolyExtFieldElem(std::move(b));
    }

    static inline constexpr PolyExtFieldElem div(const PolyExtFieldElem& x, uint256 c)
    {
        PolyExtFieldElem result;

        for (size_t i = 0; i < degree; ++i)
            result.coeffs[i] = arith.div(x.coeffs[i], c);

        return result;
    }

    static inline constexpr size_t deg(const std::vector<uint256>& v)
    {
        size_t d = v.size() - 1;

        while (d > 0 && v[d] == 0)
            --d;

        return d;
    }

    static inline constexpr std::vector<uint256> poly_rounded_div(
        const std::vector<uint256>& a, const std::vector<uint256>& b)
    {
        auto dega = deg(a);
        auto degb = deg(b);
        auto temp = a;
        auto o = std::vector<uint256>(a.size());
        if (dega >= degb)
        {
            for (size_t i = dega - degb + 1; i > 0; --i)
            {
                auto d = arith.div(temp[degb + i - 1], b[degb]);
                o[i - 1] = arith.add(o[i - 1], d);
                for (size_t c = 0; c < degb + 1; ++c)
                    temp[c + i - 1] = arith.sub(temp[c + i - 1], o[c]);
            }
        }

        return std::vector<uint256>(
            o.begin(), o.begin() + (typename std::vector<uint256>::difference_type)deg(o) + 1);
    }

    static inline PolyExtFieldElem inv(const PolyExtFieldElem& x)
    {
        std::vector<uint256> lm(degree + 1);
        lm[0] = arith.one_mont();
        std::vector<uint256> hm(degree + 1);

        std::vector<uint256> low = x.coeffs;
        low.push_back(0);

        std::vector<uint256> high(degree);
        std::copy(ModCoeffsT::MODULUS_COEFFS, ModCoeffsT::MODULUS_COEFFS + degree, high.begin());
        high.push_back(arith.one_mont());

        while (deg(low) > 0)
        {
            auto r = poly_rounded_div(high, low);
            r.resize(degree + 1);
            auto nm = hm;
            auto _new = high;

            assert(lm.size() == hm.size() && hm.size() == low.size() && low.size() == high.size() &&
                   high.size() == _new.size() && _new.size() == degree + 1);
            for (size_t i = 0; i < degree + 1; ++i)
            {
                for (size_t j = 0; j < degree + 1 - i; ++j)
                {
                    nm[i + j] = arith.sub(nm[i + j], arith.mul(lm[i], r[j]));
                    _new[i + j] = arith.sub(_new[i + j], arith.mul(low[i], r[j]));
                }
            }

            high = low;
            hm = lm;
            low = _new;
            lm = nm;
        }

        return div(PolyExtFieldElem(std::vector<uint256>(lm.begin(), lm.begin() + degree)), low[0]);
    }

    static inline constexpr PolyExtFieldElem div(
        const PolyExtFieldElem& x, const PolyExtFieldElem& y)
    {
        return mul(x, inv(y));
    }

    static inline PolyExtFieldElem one()
    {
        std::vector<uint256> _one(degree);
        _one[0] = 1;
        return PolyExtFieldElem(std::move(_one));
    }

    static inline PolyExtFieldElem one_mont()
    {
        std::vector<uint256> _one(degree);
        _one[0] = arith.one_mont();
        return PolyExtFieldElem(std::move(_one));
    }

    static inline constexpr PolyExtFieldElem zero()
    {
        return PolyExtFieldElem();
    }

    template <typename PowUintT>
    static inline constexpr PolyExtFieldElem pow(const PolyExtFieldElem& x, const PowUintT& y)
    {
        if (y == 0)
            return one();
        else if (y == 1)
            return x;
        else if (y % 2 == 0)
            return pow(mul(x, x), y / 2);
        else
            return mul(pow(mul(x, x), y / 2), x);
    }

    static inline constexpr PolyExtFieldElem neg(const PolyExtFieldElem& x)
    {
        PolyExtFieldElem result;

        for (size_t i = 0; i < degree; ++i)
            result.coeffs[i] = arith.neg(x.coeffs[i]);

        return result;
    }

    static inline constexpr bool eq(const PolyExtFieldElem& x, const PolyExtFieldElem& y)
    {
        for (size_t i = 0; i < degree; ++i)
        {
            if (x.coeffs[i] != y.coeffs[i])
                return false;
        }

        return true;
    }

    friend std::ostream& operator<<(std::ostream& os, const PolyExtFieldElem& x)
    {
        os << "["sv;

        if (!x.coeffs.empty())
        {
            for (size_t i = 0; i < x.coeffs.size() - 1; ++i)
                os << "0x"sv << hex(x.coeffs[i]) << ", ";
            os << "0x"sv << hex(x.coeffs.back());
        }

        os << "]"sv;
        return os;
    }

    PolyExtFieldElem to_mont() const
    {
        PolyExtFieldElem result;

        for (size_t i = 0; i < degree; ++i)
            result.coeffs[i] = arith.to_mont(this->coeffs[i]);

        return result;
    }

    PolyExtFieldElem from_mont() const
    {
        PolyExtFieldElem result;

        for (size_t i = 0; i < degree; ++i)
            result.coeffs[i] = arith.from_mont(this->coeffs[i]);

        return result;
    }
};

}  // namespace evmmax