# clover

A library for concolic execution of RV32 instruction set simulators.

## Motivation

This library serves as an abstraction over [KLEE][klee website]
specifically designed to ease concolic execution of RV32 instruction set
simulators. Currently, KLEE is only used as a fancy SMT Solver. Since
the majority of KLEE features is not required by this library, a bundled
version of KLEE is included which only depends on Z3 and LLVM. This
approach was also taken by [S²E][s2e klee] and eases using KLEE as a
library.

## Usage

This library has the following dependencies:

* A C++ compiler toolchain with C++17 support
* A recent version of [Z3][z3 repo] (`4.8.X` is known to work)
* A recent version of [LLVM][llvm website] (`10.0.1` is known to work)

If all dependencies have been installed this library can be added to an
existing RV32 instruction set simulator (e.g. as a git submodule).
Afterwards, it can easily be integrated into CMake-based build systems
through the [`add_subdirectory`][cmake add_subdirectory] directive.

## API

The central data types are the following C++ classes:

* `ConcolicValue`: Tuple of concrete and symbolic value with associated operations.
* `Solver`: Allows evaluating symbolic/concrete part of a `ConcolicValue`.
   Also acts as a factory for `ConcolicValue` types.
* `Trace`: Collects path conditions for the current execution.  Based on
   collected path conditions, assignment for new paths can be discovered.
* `ExecutionContext`: Used to management assignment of symbolic data to
   variable names, i.e. manages the concrete/symbolic store.

Additional convenience classes are provided. For example,
`ConcolicMemory` eases the implementation of a memory peripheral for
instruction set simulators. The `TestCase` class on the other hand can
be used to write a concrete store to a file, thereby easing replaying of
certain paths. The `BitVector` class is primarily intended for internal
use.

## Development

A pre-commit git hook for checking if files are properly formatted is
provided in `.githooks`. It can be activated using:

	$ git config --local core.hooksPath .githooks

## Acknowledgements

This work was supported in part by the German Federal Ministry of
Education and Research (BMBF) within the project Scale4Edge under
contract no.~16ME0127 and within the project VerSys under contract
no.~01IW19001.

Furthermore, this work is based heavily on KLEE. A minimally modified
version of the KLEE source is included in `./klee`. The license of this
source code is found in `./klee/LICENSE.txt`.

## License

This repository includes a bundled version of KLEE, for the license of
this code refer to `./klee/LICENSE.txt`. The clover code itself is
licenced under the following license:

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.

[s2e klee]: https://github.com/S2E/s2e/tree/master/klee
[klee website]: https://klee.github.io/
[z3 repo]: https://github.com/Z3Prover/z3
[llvm website]: https://llvm.org/
[cmake add_subdirectory]: https://cmake.org/cmake/help/latest/command/add_subdirectory.html
