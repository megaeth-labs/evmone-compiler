# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_library(compiler-rt)
target_sources(
    compiler-rt PRIVATE
    aot_eof.cpp
    aot_instructions_calls.cpp
    aot_instructions_storage.cpp
)
target_compile_features(compiler-rt PUBLIC cxx_std_23)
target_link_libraries(compiler-rt PUBLIC evmc::evmc ethash::keccak)

add_executable(compiler)
target_sources(
    compiler PRIVATE
    compiler.cpp
)
target_compile_features(compiler PUBLIC cxx_std_23)
target_link_libraries(compiler PRIVATE evmc::evmc ethash::keccak)

add_subdirectory(benchmark)
