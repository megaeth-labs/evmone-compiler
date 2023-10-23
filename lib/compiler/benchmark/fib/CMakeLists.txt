# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(fib)
target_sources(
    fib PRIVATE
    fib.cpp
)
target_compile_features(fib PUBLIC cxx_std_23)
target_include_directories(fib PRIVATE ${compiler_rt_include_dir})
target_link_libraries(fib PUBLIC compiler-rt PerfUtils)
target_compile_definitions(fib PRIVATE GAS_CHECK_LOC=1)

add_executable(fib-early-gas-check)
target_sources(
    fib-early-gas-check PRIVATE
    fib.cpp
)
target_compile_features(fib-early-gas-check PUBLIC cxx_std_23)
target_include_directories(fib-early-gas-check PRIVATE ${compiler_rt_include_dir})
target_link_libraries(fib-early-gas-check PUBLIC compiler-rt PerfUtils)
target_compile_definitions(fib-early-gas-check PRIVATE GAS_CHECK_LOC=0)

add_executable(fib-no-gas-check)
target_sources(
    fib-no-gas-check PRIVATE
    fib.cpp
)
target_compile_features(fib-no-gas-check PUBLIC cxx_std_23)
target_include_directories(fib-no-gas-check PRIVATE ${compiler_rt_include_dir})
target_link_libraries(fib-no-gas-check PUBLIC compiler-rt PerfUtils)
target_compile_definitions(fib-no-gas-check PRIVATE GAS_CHECK_OFF=1)
