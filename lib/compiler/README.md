# evmone compiler
> EVM ahead-of-time compiler based on the fast evmone interpreter

## Approach

At the high level, our compilation scheme is as follows. 
```
                        ┌───────────────┐
                        │               │
                        │ evmone opcode ├────┐
                        │    handlers   │    │
                        │               │    │
                        └───────────────┘    │
                                             │
┌──────────────┐         ┌──────────────┐    │     ┌───────────────┐         ┌──────────────────┐
│              │         │              │    │     │               │         │                  │
│ EVM bytecode ├────────►│ C++ function ├────┴────►│  Native code  ├────────►│ Execution client │
│              │   Our   │              │   C++    │  (.so file)   │ dlopen  │                  │
└──────────────┘ compiler└──────────────┘ compiler └───────────────┘         └──────────────────┘
```

First, each EVM contract is compiled into a C++ function. Inside the function, EVM opcodes are translated to function calls to evmone's corresponding opcode handlers. Most translations are trivial: only opcodes that are related to the control flow must be dealt with extra care. Specifically, we use GNU's [computed-goto](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html) extension to transfer the control flow among basic blocks of the bytecode. 

**TODO:** describe the implementation details and optimizations

Second, we rely on a C++ compiler to generate native code for our C++ function. Ideally, the C++ compiler should optimize away most of the stack operations (among other optimizations). Here is a concrete [example](https://gcc.gnu.org/onlinedocs/jit/intro/tutorial04.html#behind-the-curtain-how-does-our-code-get-optimized) how this can be done behind the curtain.

Finally, an execution client can dynamically link (and unlink) the compiled contract (a shared object file) at runtime.


## Current status

We support all EVM opcodes up till the Shanghai hard fork; this is trivial, since we are mostly reusing evmone's opcode implementations. Furthermore, adding support for new opcodes in the upcoming [EOF](https://notes.ethereum.org/@ipsilon/evm-object-format-overview) should be relatively simple.

We can also compile non-trivial contracts such as [snailtracer](https://github.com/axic/snailtracer). Unfortunately, the quality of the generated code is still far from ideal for large contracts.

## Building

The EVM compiler reuses the CMake-based build system of evmone. However, it is mandatory to use `clang++-17` or higher (other C++ compilers are not supported right now).

On a fresh Ubuntu 22.04 system, the following commands should install all the dependencies for you (we do need a relatively new `libstdc++`, hence the `gcc-12` below):
```
sudo apt update; sudo apt install cmake gcc-12 g++-12 -y
yes | sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
```

Then you can following the [build instructions](https://github.com/ethereum/evmone#building-from-source) of evmone. Just remember to point the C/C++ compilers to `clang`:
```
CC=clang-17 CXX=clang++-17 cmake -S . -B build -DEVMONE_TESTING=ON
```

## Example usages

Compile a simple hand-coded fibonacci program to a C++ function:
```
build/lib/compiler/compiler 5f35600060015b8215601b578181019150909160019003916006565b91505000
```

Run the fibonacci benchmark that computes `fib(100000000)` using the C++ function generated above:
```
build/lib/compiler/benchmark/fib/fib 100000000
```

Run the fibonacci program using the evmone interpreter:
```
build/lib/compiler/benchmark/interpreter --contract-code 5f35600060015b8215601b578181019150909160019003916006565b91505000 \
    --calldata 0000000000000000000000000000000000000000000000000000000005f5e100
```


## Preliminary result

We demonstrate the huge potential gain in performance using a simple hand-coded fibonacci program adapted from [paradigmxyz/jitevm](https://github.com/paradigmxyz/jitevm/blob/f82261fc8a1a6c1a3d40025a910ba0ce3fcaed71/src/test_data.rs#L7).

The following two tables compares the performance of various versions of the program using two different CPUs:
- `evmone interpreter`: the fastest EVM interpreter as of today;
- `evmone compiler`: our compiler prototype;
- `Manual loop inversion`: apply a simple [loop inversion](https://en.wikipedia.org/wiki/Loop_inversion) optimization on the bytecode before passing it through our compiler;
- `Elide gas check`: after loop inversion, remove the out-of-gas checks (but still keep gas metering) in the generated code;
- `Native C`: a 256-bit arithmetic fibonacci program written in C++.


*CPU: AMD EPYC™ 7543*
|             Fib(10^8) | Time (ms) | Slowdown (vs. C) | Speedup (vs. interpreter) |
|----------------------:|----------:|-----------------:|--------------------------:|
|    evmone interpreter |      4230 |          141.00x |                     1.00x |
|   **evmone compiler** |   **403** |       **13.33x** |                **10.50x** |
| Manual loop inversion |       134 |            4.44x |                    31.57x |
|       Elide gas check |        43 |            1.37x |                    98.37x |
|              Native C |        30 |            1.00x |                   141.00x |

*CPU: Intel® Xeon® Processor E5-2640 v4*
|             Fib(10^8) | Time (ms) | Slowdown (vs. C) | Speedup (vs. interpreter) |
|----------------------:|----------:|-----------------:|--------------------------:|
|    evmone interpreter |      3943 |           67.98x |                     1.00x |
|   **evmone compiler** |   **740** |       **12.73x** |                 **5.32x** |
| Manual loop inversion |       236 |            4.07x |                    16.71x |
|       Elide gas check |        59 |            1.03x |                    66.83x |
|              Native C |        58 |            1.00x |                    67.98x |


Our early results are very encouraging: the current prototype can already achieve 5-10x speedup against the fastest EVM interpreter. The speedup will be even more significant (17-32x) if we perform loop inversions automatically. Or even better, we could improve our generated code and let LLVM's [`LoopRotation` pass](https://llvm.org/docs/LoopTerminology.html#rotated-loops) do the work for us. Finally, we believe it's possible to remove the out-of-gas checks completely from the generated code and achieve C-level performance (<50% slower) ultimately.


## Limitations

The current "one-function-per-contract" scheme doesn't work well for larger contracts. The resulting C++ function is too complex for the C++ compiler to optimize effectively (the `snailtracer` example has a single function with more than 10k lines of code).

We have not attempted to optimize for the compilation speed or the resulting code size.
