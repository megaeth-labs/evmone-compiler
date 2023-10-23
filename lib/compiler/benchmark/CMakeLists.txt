# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

set(compiler_rt_include_dir ${PROJECT_SOURCE_DIR}/lib/compiler)

add_library(PerfUtils)
target_sources(
    PerfUtils PRIVATE
    Cycles.cpp
)
set_target_properties(PerfUtils PROPERTIES CXX_VISIBILITY_PRESET default)

add_executable(interpreter)
target_sources(
    interpreter PRIVATE
    interpreter.cpp
)
target_compile_features(interpreter PUBLIC cxx_std_23)
target_include_directories(interpreter PRIVATE ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(interpreter PUBLIC evmone PerfUtils)

add_subdirectory(fib)
add_subdirectory(snailtracer)
