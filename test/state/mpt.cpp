// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "mpt.hpp"
#include "rlp.hpp"
#include <algorithm>
#include <cassert>

namespace evmone::state
{
namespace
{
/// The collection of nibbles (4-bit values) representing a path in a MPT.
struct Path
{
    size_t length = 0;  // TODO: Can be converted to uint8_t.
    uint8_t nibbles[64]{};

    Path() = default;

    explicit Path(bytes_view key) noexcept : length{2 * key.size()}
    {
        assert(length <= std::size(nibbles));
        size_t i = 0;
        for (const auto b : key)
        {
            // static_cast is only needed in GCC <= 8.
            nibbles[i++] = static_cast<uint8_t>(b >> 4);
            nibbles[i++] = static_cast<uint8_t>(b & 0x0f);
        }
    }

    [[nodiscard]] Path tail(size_t pos) const noexcept
    {
        assert(pos <= length);  // MPT never requests whole path copy (pos == 0).
        Path p;
        p.length = length - pos;
        std::copy_n(&nibbles[pos], p.length, p.nibbles);
        return p;
    }

    [[nodiscard]] Path head(size_t size) const noexcept
    {
        assert(size < length);  // MPT never requests whole path copy (size == length).
        Path p;
        p.length = size;
        std::copy_n(nibbles, size, p.nibbles);
        return p;
    }

    [[nodiscard]] bytes encode(bool extended) const
    {
        bytes bs;
        const auto is_even = length % 2 == 0;
        if (is_even)
            bs.push_back(0x00);
        else
            bs.push_back(0x10 | nibbles[0]);
        for (size_t i = is_even ? 0 : 1; i < length; ++i)
        {
            const auto h = nibbles[i++];
            const auto l = nibbles[i];
            assert(h <= 0x0f);
            assert(l <= 0x0f);
            bs.push_back(static_cast<uint8_t>((h << 4) | l));
        }
        if (!extended)
            bs[0] |= 0x20;
        return bs;
    }
};
}  // namespace

/// The MPT Node.
///
/// The implementation is based on StackTrie from go-ethereum.
// clang-tidy-16 bug: https://github.com/llvm/llvm-project/issues/50006
// NOLINTNEXTLINE(bugprone-reserved-identifier)
class MPTNode
{
    enum class Kind : uint8_t
    {
        leaf,
        branch
    };

    static constexpr size_t num_children = 16;

    Kind m_kind = Kind::leaf;
    Path m_path;
    bytes m_value;
    std::unique_ptr<MPTNode> m_children[num_children];

    explicit MPTNode(Kind kind, const Path& path = {}, bytes&& value = {}) noexcept
      : m_kind{kind}, m_path{path}, m_value{std::move(value)}
    {}

    /// Creates a branch node out of two children and optionally extends it with an extended
    /// node in case the path is not empty.
    static MPTNode ext_branch(const Path& path, size_t idx1, std::unique_ptr<MPTNode> child1,
        size_t idx2, std::unique_ptr<MPTNode> child2) noexcept
    {
        assert(idx1 != idx2);
        assert(idx1 < num_children);
        assert(idx2 < num_children);

        MPTNode br{Kind::branch};
        br.m_path = path;
        br.m_children[idx1] = std::move(child1);
        br.m_children[idx2] = std::move(child2);
        return br;
    }

    /// Finds the position at witch two paths differ.
    static size_t mismatch(const Path& p1, const Path& p2) noexcept
    {
        assert(p1.length <= p2.length);
        return static_cast<size_t>(
            std::mismatch(p1.nibbles, p1.nibbles + p1.length, p2.nibbles).first - p1.nibbles);
    }

public:
    MPTNode() = default;

    /// Creates new leaf node.
    static std::unique_ptr<MPTNode> leaf(const Path& path, bytes&& value) noexcept
    {
        return std::make_unique<MPTNode>(MPTNode{Kind::leaf, path, std::move(value)});
    }

    void insert(const Path& path, bytes&& value);

    [[nodiscard]] bytes encode() const;
};

void MPTNode::insert(const Path& path, bytes&& value)  // NOLINT(misc-no-recursion)
{
    // The insertion is all about branch nodes. In happy case we will find an empty slot
    // in an existing branch node. Otherwise, we need to create new branch node
    // (possibly with an adjusted extended node) and transform existing nodes around it.

    switch (m_kind)
    {
    case Kind::branch:
    {
        const auto mismatch_pos = mismatch(m_path, path);

        if (mismatch_pos == m_path.length)  // Paths match: go into the child.
        {
            const auto sub_path = path.tail(mismatch_pos);
            const auto idx = sub_path.nibbles[0];
            auto& child = m_children[idx];
            if (!child)
                child = leaf(sub_path.tail(1), std::move(value));
            else
                child->insert(sub_path.tail(1), std::move(value));
            return;
        }

        const auto orig_idx = m_path.nibbles[mismatch_pos];
        const auto new_idx = path.nibbles[mismatch_pos];

        // The original branch node must be pushed down, possible extended with
        // the adjusted extended node if the path split point is not directly at the branch
        // node. Clang Analyzer bug: https://github.com/llvm/llvm-project/issues/47814
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)

        auto down_branch = std::make_unique<MPTNode>();
        down_branch->m_kind = Kind::branch;
        // optional_ext(m_path.tail(mismatch_pos + 1), *this);
        down_branch->m_path = m_path.tail(mismatch_pos + 1);
        for (size_t i = 0; i < num_children; ++i)
            down_branch->m_children[i] = std::move(m_children[i]);

        auto new_leaf = leaf(path.tail(mismatch_pos + 1), std::move(value));
        *this = ext_branch(m_path.head(mismatch_pos), orig_idx, std::move(down_branch), new_idx,
            std::move(new_leaf));
        break;
    }

    case Kind::leaf:
    {
        assert(m_path.length != 0);  // Leaf must have non-empty path.

        const auto mismatch_pos = mismatch(m_path, path);
        assert(mismatch_pos != m_path.length);  // Paths must be different.

        const auto orig_idx = m_path.nibbles[mismatch_pos];
        const auto new_idx = path.nibbles[mismatch_pos];
        auto orig_leaf = leaf(m_path.tail(mismatch_pos + 1), std::move(m_value));
        auto new_leaf = leaf(path.tail(mismatch_pos + 1), std::move(value));
        *this = ext_branch(m_path.head(mismatch_pos), orig_idx, std::move(orig_leaf), new_idx,
            std::move(new_leaf));
        break;
    }

    default:
        assert(false);
    }
}

/// Encodes a node and optionally hashes the encoded bytes
/// if their length exceeds the specified threshold.
static bytes encode_child(const MPTNode& child) noexcept  // NOLINT(misc-no-recursion)
{
    if (auto e = child.encode(); e.size() < 32)
        return e;  // "short" node
    else
        return rlp::encode(keccak256(e));
}

bytes MPTNode::encode() const  // NOLINT(misc-no-recursion)
{
    bytes encoded;
    switch (m_kind)
    {
    case Kind::leaf:
    {
        encoded = rlp::encode(m_path.encode(false)) + rlp::encode(m_value);
        break;
    }
    case Kind::branch:
    {
        bytes branch;
        static constexpr uint8_t empty = 0x80;  // encoded empty child

        for (const auto& child : m_children)
        {
            if (child)
                branch += encode_child(*child);
            else
                branch += empty;
        }
        branch += empty;  // end indicator

        if (m_path.length == 0)
        {
            encoded = branch;
            break;
        }

        branch = rlp::internal::wrap_list(branch);
        if (branch.size() >= 32)
            branch = rlp::encode(keccak256(branch));

        encoded = rlp::encode(m_path.encode(true)) + branch;
        break;
    }
    }

    return rlp::internal::wrap_list(encoded);
}


MPT::MPT() noexcept = default;
MPT::~MPT() noexcept = default;

void MPT::insert(bytes_view key, bytes&& value)
{
    if (m_root == nullptr)
        m_root = MPTNode::leaf(Path{key}, std::move(value));
    else
        m_root->insert(Path{key}, std::move(value));
}

[[nodiscard]] hash256 MPT::hash() const
{
    if (m_root == nullptr)
        return emptyMPTHash;
    return keccak256(m_root->encode());
}

}  // namespace evmone::state
