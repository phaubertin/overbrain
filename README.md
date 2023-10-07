# Overbrain

An overdesigned [Brainf*ck](https://en.wikipedia.org/wiki/Brainfuck) interpreter, compiler and
Just-In-Time (JIT) compiler/runtime.

## How to Build

To build, you need a Linux machine with GNU make and GCC.

Just run `make` in the top level directory. This builds two binaries in the `src/` directory: `bf`
and `bfc`.

## How to Install (Optional)

Running `make install` in the top level directory will install `bf` and `bfc` in `/usr/local/bin/`.

Alternatively, the binaries can be run directly from their original location or copied anywhere.

## How to Run

This software runs on any Linux machine and does not have dependencies beyond the standard C/POSIX
library.

Usage: `(bf|bfc) [options] program`

The build process builds two binaries: `bf` and `bfc`. These two binaries accept all the same
command line arguments and behave the same except that, by default, `bf` runs the program as if the
`-jit` command line argument was specified whereas `bfc` compiles the program as if the `-compile`
argument was specified. For both binaries, this default can be overwritten by command line
arguments.

### Options to Run a Program

The input program is run when one of the following options is specified:

* `-jit` runs the program using the Just-In-Time (JIT) compiler. This is the default for `bf`.
* `-tree` runs the program using the tree interpreter.
* `-slow` runs the program using the "slow" interpreter, which is a a naive interpreter that
interprets the program text directly.

The following optimization options apply to both the JIT compiler and the tree interpreter.
These options are ignored if the slow interpreter is selected:

* Specifying the `-O0` option disables optimizations while `-O1`, `-O2` or `-O3` enables them.
`-O1`, `-O2` and `-O3` are synonyms. The default is `-O3`.
* The `-no-check` option disables bound checks. Using this option is not recommended because it
makes the program unsafe and the performance gain is marginal.

### Options to Compile a Program

The input program is compiled when the `-compile` options is specified. This is the default for
`bfc`.

The`-o` option specifies the output file. If this option is omitted, the default behaviour is to
output the compiled program to standard output.

The `-backend` options selects the compilation backend, which determines the format of the output
file:

* `-backend elf64` compiles the program to an ELF64 binary that targets a 64-bit x86 Linux system. This is
the default.
* `-backend nasm` compiles the program to assembly code intended for the Netwide Assembler (NASM).
* `-backend c` compiles the program to C source code.

Optimization options:

* Specifying the `-O0` option disables optimizations while `-O1`, `-O2` or `-O3` enables them.
`-O1`, `-O2` and `-O3` are synonyms. The default is `-O3`.
* The `-no-check` option disables bound checks. Using this option is not recommended because it
makes the program unsafe and the performance gain is marginal.
