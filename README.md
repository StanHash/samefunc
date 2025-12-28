# samefunc

Finds functions with loosely the same ASM in ARM32 ELF object files. It looks at ASM but ignores relocations and some immediate operands. It only knows to compare thumb asm for now.

This is meant as a tool for aiding matching decompilation. It is useful for analysing and comparing decompiled and yet to be decompiled code, as well as finding shared code between different binaries.

## Build

You need elf.h, (GNU) make and a C++20 compiler.

    make

## Usage

    ./samefunc ELF...

For example:

    ./samefunc build/src/*.o
