// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <test/state/ethash_difficulty.hpp>

using namespace evmone::state;

struct DifficultyTest  // NOLINT(clang-analyzer-optin.performance.Padding)
{
    evmc_revision rev;
    const char* name;
    int64_t block_number;
    int64_t difficulty;
    int64_t timestamp;
    int64_t parent_difficulty;
    int64_t parent_timestamp;
    bool parent_has_ommers;
};

/// Example difficulty tests from
/// https://github.com/ethereum/tests/blob/develop/DifficultyTests.
static constexpr DifficultyTest tests[] = {
    {
        EVMC_FRONTIER,
        "DifficultyTest1",
        0x0186a0,
        0x6a8d5758858f3fb6,
        0x75311a08b,
        0x6a8007579a9bec39,
        0x75311a08a,
        false,
    },
    {
        EVMC_FRONTIER,
        "DifficultyTest1040",
        0x10c8e0,
        0x3857fe2e57047922,
        0x3558049dc,
        0x385f0a0f98f79614,
        0x3558049c8,
        true,
    },
    {
        EVMC_FRONTIER,
        "DifficultyTest1031",
        0x030d40,
        0x7b435a6e9d83b81e,
        0x2442b7295,
        0x7b52c4c7366a856d,
        0x2442b7281,
        true,
    },
    {
        EVMC_HOMESTEAD,
        "DifficultyTest1",
        0x0186a0,
        0x6ab7534e3bcfec27,
        0x68450ae0d,
        0x6aa9fe0e7a00ac12,
        0x68450ae0c,
        false,
    },
    {
        EVMC_HOMESTEAD,
        "DifficultyTest1040",
        0x10c8e0,
        0x024bf6f60ecc847d,
        0x7b389fe96,
        0x024c407e1e905487,
        0x7b389fe82,
        true,
    },
    {
        EVMC_HOMESTEAD,
        "DifficultyTest1031",
        0x030d40,
        0x39699ae4587a0b05,
        0x3b907b4f4,
        0x3970c8fd78291026,
        0x3b907b4e0,
        true,
    },
    {
        EVMC_BYZANTIUM,
        "DifficultyTest1",
        0x0186a0,
        0x69702c7f2c9fad14,
        0x28d214819,
        0x6963001f28ba95c2,
        0x28d214818,
        false,
    },
    {
        EVMC_BYZANTIUM,
        "DifficultyTest1038",
        0x0dbba0,
        0x79f2cbb8c97579b0,
        0x72e440371,
        0x79f2cbb8c97579b0,
        0x72e44035d,
        true,
    },
    {
        EVMC_BERLIN,
        "DifficultyTest1",
        0x186a0,
        0x56c67d1e106966c3,
        0x63ed689e9,
        0x56bba5a95b3dff04,
        0x63ed689e8,
        false,
    },
    {
        EVMC_BERLIN,
        "DifficultyTest1040",
        0x10c8e0,
        0x68f7512123928555,
        0x617ec9fcc,
        0x68f7512123928555,
        0x617ec9fb8,
        true,
    },
};

TEST(state_difficulty, tests)
{
    for (const auto& t : tests)
    {
        const auto difficulty = calculate_difficulty(t.parent_difficulty, t.parent_has_ommers,
            t.parent_timestamp, t.timestamp, t.block_number, t.rev);
        EXPECT_EQ(difficulty, t.difficulty) << t.rev << "/" << t.name;
    }
}
