// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/errors.hpp>
#include <test/state/state.hpp>
#include <test/utils/utils.hpp>

using namespace evmone::state;
using namespace evmc::literals;

TEST(state_tx, validate_nonce)
{
    const BlockInfo bi{.gas_limit = 0x989680,
        .coinbase = 0x01_address,
        .prev_randao = {},
        .base_fee = 0x0a,
        .withdrawals = {}};
    const Account acc{.nonce = 1, .balance = 0xe8d4a51000};
    Transaction tx{
        .data = {},
        .gas_limit = 60000,
        .max_gas_price = bi.base_fee,
        .max_priority_gas_price = 0,
        .sender = 0x02_address,
        .to = {},
        .value = 0,
        .access_list = {},
        .nonce = 1,
        .r = 0,
        .s = 0,
        .initcodes = {},
    };
    ASSERT_FALSE(
        holds_alternative<std::error_code>(validate_transaction(acc, bi, tx, EVMC_BERLIN, 60000)));

    tx.nonce = 0;
    EXPECT_EQ(
        std::get<std::error_code>(validate_transaction(acc, bi, tx, EVMC_BERLIN, 60000)).message(),
        "nonce too low");

    tx.nonce = 2;
    EXPECT_EQ(
        std::get<std::error_code>(validate_transaction(acc, bi, tx, EVMC_BERLIN, 60000)).message(),
        "nonce too high");
}

TEST(state_tx, validate_sender)
{
    BlockInfo bi{.gas_limit = 0x989680,
        .coinbase = 0x01_address,
        .prev_randao = {},
        .base_fee = 0,
        .withdrawals = {}};
    const Account acc{.nonce = 0, .balance = 0};
    Transaction tx{
        .data = {},
        .gas_limit = 60000,
        .max_gas_price = bi.base_fee,
        .max_priority_gas_price = 0,
        .sender = 0x02_address,
        .to = {},
        .value = 0,
        .access_list = {},
        .nonce = 0,
        .r = 0,
        .s = 0,
        .initcodes = {},
    };

    ASSERT_FALSE(
        holds_alternative<std::error_code>(validate_transaction(acc, bi, tx, EVMC_BERLIN, 60000)));

    bi.base_fee = 1;

    EXPECT_EQ(
        std::get<std::error_code>(validate_transaction(acc, bi, tx, EVMC_LONDON, 60000)).message(),
        "max fee per gas less than block base fee");

    tx.max_gas_price = bi.base_fee;

    EXPECT_EQ(
        std::get<std::error_code>(validate_transaction(acc, bi, tx, EVMC_LONDON, 60000)).message(),
        "insufficient funds for gas * price + value");
}

TEST(state_tx, validate_data)
{
    BlockInfo bi{.gas_limit = 0x989680,
        .coinbase = 0x01_address,
        .prev_randao = {},
        .base_fee = 0x0a,
        .withdrawals = {}};
    const Account acc{.nonce = 1, .balance = 0xe8d4a51000};
    Transaction tx{
        .data = "EF00 01 010004 0200010001 030004 00 00000000 00 AABBCCDD"_hex,
        .gas_limit = 60000,
        .max_gas_price = bi.base_fee,
        .max_priority_gas_price = 0,
        .sender = 0x02_address,
        .to = {},
        .value = 0,
        .access_list = {},
        .nonce = 1,
        .r = 0,
        .s = 0,
        .initcodes = {},
    };

    ASSERT_FALSE(
        holds_alternative<std::error_code>(validate_transaction(acc, bi, tx, EVMC_CANCUN, 60000)));

    EXPECT_EQ(
        std::get<std::error_code>(validate_transaction(acc, bi, tx, EVMC_PRAGUE, 60000)).message(),
        "EOF in creation transaction");

    tx.data = "00"_hex;

    ASSERT_FALSE(
        holds_alternative<std::error_code>(validate_transaction(acc, bi, tx, EVMC_PRAGUE, 60000)));
}
