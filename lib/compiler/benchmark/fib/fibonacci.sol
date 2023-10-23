// SPDX-License-Identifier: GPL-3.0
pragma solidity ^0.8.17;

// This contract is compiled on remix.ethereum.org using the configuration in
// compiler_config.json to obtain the bytecode of "fib-solidity" in ./fib.cpp.
contract Fibonacci {

    function fibonacci(uint N) external pure returns(uint) {
        unchecked {
            if (N < 2)
                return N;

            uint a = 0;
            uint b = 1;
            for (uint i = 2; i <= N; i++) {
                uint c = a + b;
                a = b;
                b = c;
            }
            return b;
        }
    }

}
