# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

hunter_add_package(nlohmann_json)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(evmone-blockchaintestutils STATIC)
add_library(evmone::blockchaintestutils ALIAS evmone-blockchaintestutils)
target_compile_features(evmone-blockchaintestutils PUBLIC cxx_std_20)
target_link_libraries(evmone-blockchaintestutils PRIVATE evmone::state nlohmann_json::nlohmann_json)
target_include_directories(evmone-blockchaintestutils PRIVATE ${evmone_private_include_dir})
target_sources(
    evmone-blockchaintestutils PRIVATE
    blockchaintest.hpp
    blockchaintest_loader.cpp
)

add_executable(evmone-blockchaintest)
target_link_libraries(evmone-blockchaintest PRIVATE evmone::blockchaintestutils evmone evmone::statetestutils GTest::gtest)
target_include_directories(evmone-blockchaintest PRIVATE ${evmone_private_include_dir})
target_sources(
    evmone-blockchaintest PRIVATE
    blockchaintest.hpp
    blockchaintest.cpp
    blockchaintest_loader.cpp
    blockchaintest_runner.cpp
)
