# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(snailtracer)
target_sources(
    snailtracer PRIVATE
    snailtracer.cpp
)
target_compile_features(snailtracer PUBLIC cxx_std_23)
target_include_directories(snailtracer PRIVATE ${compiler_rt_include_dir})
target_link_libraries(snailtracer PUBLIC compiler-rt PerfUtils)
target_compile_definitions(snailtracer PRIVATE GAS_CHECK_LOC=1)

add_executable(snailtracer-no-gas-check)
target_sources(
    snailtracer-no-gas-check PRIVATE
    snailtracer.cpp
)
target_compile_features(snailtracer-no-gas-check PUBLIC cxx_std_23)
target_include_directories(snailtracer-no-gas-check PRIVATE ${compiler_rt_include_dir})
target_link_libraries(snailtracer-no-gas-check PUBLIC compiler-rt PerfUtils)
target_compile_definitions(snailtracer-no-gas-check PRIVATE GAS_CHECK_OFF=1)
