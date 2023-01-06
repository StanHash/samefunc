#pragma once

#include "types.hh"

#include "func.hh"

void GetFunctionsFromElf(
    vec<Func> & out, vec<u8> const & elf_data, bool lax = true);

vec<Func> GetFunctionsFromElf(vec<u8> const & elf_data, bool lax = true);
