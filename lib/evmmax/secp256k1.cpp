#include "secp256k1.hpp"

namespace evmmax::bn254
{
bool is_at_infinity(const uint256& x, const uint256& y, const uint256& z) noexcept;

std::tuple<uint256, uint256, uint256> point_addition_a0(const evmmax::ModArith<uint256>& s,
    const uint256& x1, const uint256& y1, const uint256& z1, const uint256& x2, const uint256& y2,
    const uint256& z2, const uint256& b3) noexcept;

std::tuple<uint256, uint256, uint256> point_doubling_a0(const evmmax::ModArith<uint256>& s,
    const uint256& x, const uint256& y, const uint256& z, const uint256& b3) noexcept;

std::tuple<uint256, uint256, uint256> point_addition_mixed_a0(const evmmax::ModArith<uint256>& s,
    const uint256& x1, const uint256& y1, const uint256& x2, const uint256& y2,
    const uint256& b3) noexcept;
}  // namespace evmmax::bn254

namespace evmmax::secp256k1
{
// Computes z = 1/x (mod p) and returns it.
uint256 inv(const ModArith<uint256>& s, const uint256& x) noexcept
{
    uint256 z;
    // Inversion computation is derived from the addition chain:
    //
    // _10     = 2*1
    // _100    = 2*_10
    // _101    = 1 + _100
    // _111    = _10 + _101
    // _1110   = 2*_111
    // _111000 = _1110 << 2
    // _111111 = _111 + _111000
    // i13     = _111111 << 4 + _1110
    // x12     = i13 << 2 + _111
    // x22     = x12 << 10 + i13 + 1
    // i29     = 2*x22
    // i31     = i29 << 2
    // i54     = i31 << 22 + i31
    // i122    = (i54 << 20 + i29) << 46 + i54
    // x223    = i122 << 110 + i122 + _111
    // i269    = ((x223 << 23 + x22) << 7 + _101) << 3
    // return    _101 + i269
    //
    // Operations: 255 squares 15 multiplies
    //
    // Generated by github.com/mmcloughlin/addchain v0.4.0.

    // Allocate Temporaries.
    uint256 t0;
    uint256 t1;
    uint256 t2;
    uint256 t3;
    uint256 t4;
    // Step 1: t0 = x^0x2
    t0 = s.mul(x, x);

    // Step 2: z = x^0x4
    z = s.mul(t0, t0);

    // Step 3: z = x^0x5
    z = s.mul(x, z);

    // Step 4: t1 = x^0x7
    t1 = s.mul(t0, z);

    // Step 5: t0 = x^0xe
    t0 = s.mul(t1, t1);

    // Step 7: t2 = x^0x38
    t2 = s.mul(t0, t0);
    for (int i = 1; i < 2; ++i)
        t2 = s.mul(t2, t2);

    // Step 8: t2 = x^0x3f
    t2 = s.mul(t1, t2);

    // Step 12: t2 = x^0x3f0
    for (int i = 0; i < 4; ++i)
        t2 = s.mul(t2, t2);

    // Step 13: t0 = x^0x3fe
    t0 = s.mul(t0, t2);

    // Step 15: t2 = x^0xff8
    t2 = s.mul(t0, t0);
    for (int i = 1; i < 2; ++i)
        t2 = s.mul(t2, t2);

    // Step 16: t2 = x^0xfff
    t2 = s.mul(t1, t2);

    // Step 26: t2 = x^0x3ffc00
    for (int i = 0; i < 10; ++i)
        t2 = s.mul(t2, t2);

    // Step 27: t0 = x^0x3ffffe
    t0 = s.mul(t0, t2);

    // Step 28: t0 = x^0x3fffff
    t0 = s.mul(x, t0);

    // Step 29: t3 = x^0x7ffffe
    t3 = s.mul(t0, t0);

    // Step 31: t2 = x^0x1fffff8
    t2 = s.mul(t3, t3);
    for (int i = 1; i < 2; ++i)
        t2 = s.mul(t2, t2);

    // Step 53: t4 = x^0x7ffffe000000
    t4 = s.mul(t2, t2);
    for (int i = 1; i < 22; ++i)
        t4 = s.mul(t4, t4);

    // Step 54: t2 = x^0x7ffffffffff8
    t2 = s.mul(t2, t4);

    // Step 74: t4 = x^0x7ffffffffff800000
    t4 = s.mul(t2, t2);
    for (int i = 1; i < 20; ++i)
        t4 = s.mul(t4, t4);

    // Step 75: t3 = x^0x7fffffffffffffffe
    t3 = s.mul(t3, t4);

    // Step 121: t3 = x^0x1ffffffffffffffff800000000000
    for (int i = 0; i < 46; ++i)
        t3 = s.mul(t3, t3);

    // Step 122: t2 = x^0x1fffffffffffffffffffffffffff8
    t2 = s.mul(t2, t3);

    // Step 232: t3 = x^0x7ffffffffffffffffffffffffffe0000000000000000000000000000
    t3 = s.mul(t2, t2);
    for (int i = 1; i < 110; ++i)
        t3 = s.mul(t3, t3);

    // Step 233: t2 = x^0x7ffffffffffffffffffffffffffffffffffffffffffffffffffffff8
    t2 = s.mul(t2, t3);

    // Step 234: t1 = x^0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffff
    t1 = s.mul(t1, t2);

    // Step 257: t1 = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffff800000
    for (int i = 0; i < 23; ++i)
        t1 = s.mul(t1, t1);

    // Step 258: t0 = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff
    t0 = s.mul(t0, t1);

    // Step 265: t0 = x^0x1fffffffffffffffffffffffffffffffffffffffffffffffffffffffdfffff80
    for (int i = 0; i < 7; ++i)
        t0 = s.mul(t0, t0);

    // Step 266: t0 = x^0x1fffffffffffffffffffffffffffffffffffffffffffffffffffffffdfffff85
    t0 = s.mul(z, t0);

    // Step 269: t0 = x^0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc28
    for (int i = 0; i < 3; ++i)
        t0 = s.mul(t0, t0);

    // Step 270: z = x^0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2d
    z = s.mul(z, t0);

    return z;
}

namespace
{

std::tuple<uint256, uint256> from_proj(
    const evmmax::ModArith<uint256>& s, const uint256& x, const uint256& y, const uint256& z)
{
    auto z_inv = inv(s, z);
    return {s.mul(x, z_inv), s.mul(y, z_inv)};
}

}  // namespace

Point secp256k1_add(const Point& pt1, const Point& pt2) noexcept
{
    using namespace evmmax::bn254;
    if (is_at_infinity(pt1))
        return pt2;
    if (is_at_infinity(pt2))
        return pt1;

    const evmmax::ModArith s{Secp256K1Mod};

    const auto x1 = s.to_mont(pt1.x);
    const auto y1 = s.to_mont(pt1.y);

    const auto x2 = s.to_mont(pt2.x);
    const auto y2 = s.to_mont(pt2.y);

    // b3 == 21 for y^2 == x^3 + 7
    const auto b3 = s.to_mont(21);
    auto [x3, y3, z3] = point_addition_mixed_a0(s, x1, y1, x2, y2, b3);

    std::tie(x3, y3) = from_proj(s, x3, y3, z3);

    return {s.from_mont(x3), s.from_mont(y3)};
}

Point secp256k1_mul(const Point& pt, const uint256& c) noexcept
{
    using namespace evmmax::bn254;
    if (is_at_infinity(pt))
        return pt;

    if (c == 0)
        return {0, 0};

    const evmmax::ModArith s{Secp256K1Mod};

    auto _1_mont = s.to_mont(1);

    uint256 x0 = 0;
    uint256 y0 = _1_mont;
    uint256 z0 = 0;

    uint256 x1 = s.to_mont(pt.x);
    uint256 y1 = s.to_mont(pt.y);
    uint256 z1 = _1_mont;

    auto b3 = s.to_mont(21);

    auto first_significant_met = false;

    for (int i = 255; i >= 0; --i)
    {
        const uint256 d = c & (uint256{1} << i);
        if (d == 0)
        {
            if (first_significant_met)
            {
                std::tie(x1, y1, z1) = point_addition_a0(s, x0, y0, z0, x1, y1, z1, b3);
                std::tie(x0, y0, z0) = point_doubling_a0(s, x0, y0, z0, b3);
                // std::tie(x0, y0, z0) = point_addition_a0(s, x0, y0, z0, x0, y0, z0, b3);
            }
        }
        else
        {
            std::tie(x0, y0, z0) = point_addition_a0(s, x0, y0, z0, x1, y1, z1, b3);
            std::tie(x1, y1, z1) = point_doubling_a0(s, x1, y1, z1, b3);
            first_significant_met = true;
            // std::tie(x1, y1, z1) = point_addition_a0(s, x1, y1, z1, x1, y1, z1, b3);
        }
    }

    std::tie(x0, y0) = from_proj(s, x0, y0, z0);

    return {s.from_mont(x0), s.from_mont(y0)};
}

bool validate(const Point& pt) noexcept
{
    if (is_at_infinity(pt))
        return true;

    const evmmax::ModArith s{Secp256K1Mod};
    const auto xm = s.to_mont(pt.x);
    const auto ym = s.to_mont(pt.y);
    const auto y2 = s.mul(ym, ym);
    const auto x2 = s.mul(xm, xm);
    const auto x3 = s.mul(x2, xm);
    const auto _3 = s.to_mont(3);
    const auto x3_3 = s.add(x3, _3);
    return y2 == x3_3;
}


std::optional<uint256> sqrt(const ModArith<uint256>& s, const uint256& x) noexcept
{
    uint256 z;
    // Inversion computation is derived from the addition chain:
    //
    // _10      = 2*1
    // _11      = 1 + _10
    // _1100    = _11 << 2
    // _1111    = _11 + _1100
    // _11110   = 2*_1111
    // _11111   = 1 + _11110
    // _1111100 = _11111 << 2
    // _1111111 = _11 + _1111100
    // x11      = _1111111 << 4 + _1111
    // x22      = x11 << 11 + x11
    // x27      = x22 << 5 + _11111
    // x54      = x27 << 27 + x27
    // x108     = x54 << 54 + x54
    // x216     = x108 << 108 + x108
    // x223     = x216 << 7 + _1111111
    // return     ((x223 << 23 + x22) << 6 + _11) << 2
    //
    // Operations: 253 squares 13 multiplies
    //
    // Generated by github.com/mmcloughlin/addchain v0.4.0.

    // Allocate Temporaries.
    uint256 t0;
    uint256 t1;
    uint256 t2;
    uint256 t3;
    // Step 1: z = x^0x2
    z = s.mul(x, x);

    // Step 2: z = x^0x3
    z = s.mul(x, z);

    // Step 4: t0 = x^0xc
    t0 = s.mul(z, z);
    for (int i = 1; i < 2; ++i)
        t0 = s.mul(t0, t0);

    // Step 5: t0 = x^0xf
    t0 = s.mul(z, t0);

    // Step 6: t1 = x^0x1e
    t1 = s.mul(t0, t0);

    // Step 7: t2 = x^0x1f
    t2 = s.mul(x, t1);

    // Step 9: t1 = x^0x7c
    t1 = s.mul(t2, t2);
    for (int i = 1; i < 2; ++i)
        t1 = s.mul(t1, t1);

    // Step 10: t1 = x^0x7f
    t1 = s.mul(z, t1);

    // Step 14: t3 = x^0x7f0
    t3 = s.mul(t1, t1);
    for (int i = 1; i < 4; ++i)
        t3 = s.mul(t3, t3);

    // Step 15: t0 = x^0x7ff
    t0 = s.mul(t0, t3);

    // Step 26: t3 = x^0x3ff800
    t3 = s.mul(t0, t0);
    for (int i = 1; i < 11; ++i)
        t3 = s.mul(t3, t3);

    // Step 27: t0 = x^0x3fffff
    t0 = s.mul(t0, t3);

    // Step 32: t3 = x^0x7ffffe0
    t3 = s.mul(t0, t0);
    for (int i = 1; i < 5; ++i)
        t3 = s.mul(t3, t3);

    // Step 33: t2 = x^0x7ffffff
    t2 = s.mul(t2, t3);

    // Step 60: t3 = x^0x3ffffff8000000
    t3 = s.mul(t2, t2);
    for (int i = 1; i < 27; ++i)
        t3 = s.mul(t3, t3);

    // Step 61: t2 = x^0x3fffffffffffff
    t2 = s.mul(t2, t3);

    // Step 115: t3 = x^0xfffffffffffffc0000000000000
    t3 = s.mul(t2, t2);
    for (int i = 1; i < 54; ++i)
        t3 = s.mul(t3, t3);

    // Step 116: t2 = x^0xfffffffffffffffffffffffffff
    t2 = s.mul(t2, t3);

    // Step 224: t3 = x^0xfffffffffffffffffffffffffff000000000000000000000000000
    t3 = s.mul(t2, t2);
    for (int i = 1; i < 108; ++i)
        t3 = s.mul(t3, t3);

    // Step 225: t2 = x^0xffffffffffffffffffffffffffffffffffffffffffffffffffffff
    t2 = s.mul(t2, t3);

    // Step 232: t2 = x^0x7fffffffffffffffffffffffffffffffffffffffffffffffffffff80
    for (int i = 0; i < 7; ++i)
        t2 = s.mul(t2, t2);

    // Step 233: t1 = x^0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffff
    t1 = s.mul(t1, t2);

    // Step 256: t1 = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffff800000
    for (int i = 0; i < 23; ++i)
        t1 = s.mul(t1, t1);

    // Step 257: t0 = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff
    t0 = s.mul(t0, t1);

    // Step 263: t0 = x^0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc0
    for (int i = 0; i < 6; ++i)
        t0 = s.mul(t0, t0);

    // Step 264: z = x^0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc3
    z = s.mul(z, t0);

    // Step 266: z = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff0c
    for (int i = 0; i < 2; ++i)
        z = s.mul(z, z);

    const auto z2 = s.mul(z, z);

    return (z2 == x ? std::make_optional(z) : std::nullopt);
}

std::optional<Point> secp256k1_ecdsa_recover(
    const ethash::hash256& e, const uint256& r, const uint256& s, bool v) noexcept
{
    const ModArith<uint256> m{Secp256K1Mod};

    // Follows
    // https://en.wikipedia.org/wiki/Elliptic_Curve_Digital_Signature_Algorithm#Public_key_recovery

    // 1. Validate r and s are within [1, n-1].
    if (r == 0 || r >= Secp256K1N || s == 0 || s >= Secp256K1N)
        return std::nullopt;

    // 2. Calculate y coordinate of R from r and v.
    const auto r_mont = m.to_mont(r);
    const auto y = sec256k1_calculate_y(m, r_mont, v);
    if (!y.has_value())
        return std::nullopt;

    // 3. Hash of the message is already calculated in e.

    // 4. Convert hash e to z field element by doing z = e % n.
    //    https://www.rfc-editor.org/rfc/rfc6979#section-2.3.2
    //    We can do this by n - e because n > 2^255.
    static_assert(Secp256K1Mod > 1_u256 << 255);
    auto z = intx::be::load<uint256>(e.bytes);
    if (z >= Secp256K1Mod)
        z -= Secp256K1Mod;

    // 5. Calculate u1 and u2.
    const auto r_inv = inv(m, r_mont);

    const auto z_mont = m.to_mont(z);
    const auto z_neg = m.sub(0, z_mont);
    const auto u1_mont = m.mul(z_neg, r_inv);
    const auto u1 = m.from_mont(u1_mont);

    const auto s_mont = m.to_mont(s);
    const auto u2_mont = m.mul(s_mont, r_inv);
    const auto u2 = m.from_mont(u2_mont);

    // 6. Calculate public key point Q.
    const Point R{r, *y};
    static constexpr Point G{
        0x79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798_u256,
        0x483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8_u256};
    const Point T1 = secp256k1_mul(G, u1);
    const Point T2 = secp256k1_mul(R, u2);
    const Point Q = secp256k1_add(T1, T2);

    // Any other validity check needed?
    if (is_at_infinity(Q))
        return std::nullopt;

    return std::make_optional(Q);
}

std::optional<uint256> sec256k1_calculate_y(
    const ModArith<uint256>& s, const uint256& x, bool is_odd) noexcept
{
    static const auto Sec256k1_b = s.to_mont(7);

    // Calculate sqrt(x^3 + 7)
    const auto x3 = s.mul(s.mul(x, x), x);
    const auto y = sqrt(s, s.add(x3, Sec256k1_b));
    if (!y.has_value())
        return std::nullopt;

    // Negate if different oddity requested
    const auto y_is_odd = s.from_mont(*y) & 1;
    return (is_odd == y_is_odd ? *y : s.sub(0, *y));
}

evmc::address secp256k1_point_to_address(const Point& pt) noexcept
{
    // This performs Ethereum's address hashing on an uncompressed pubkey.
    uint8_t serialized[64];
    intx::be::unsafe::store(serialized, pt.x);
    intx::be::unsafe::store(serialized + 32, pt.y);

    const auto hashed = ethash::keccak256(serialized, sizeof(serialized));
    evmc::address ret{};
    std::memcpy(ret.bytes, hashed.bytes + 12, 20);

    return ret;
}

std::optional<evmc::address> ecrecover(
    const ethash::hash256& e, const uint256& r, const uint256& s, bool v) noexcept
{
    const auto point = secp256k1_ecdsa_recover(e, r, s, v);
    if (!point.has_value())
        return std::nullopt;

    return std::make_optional(secp256k1_point_to_address(*point));
}
}  // namespace evmmax::secp256k1
