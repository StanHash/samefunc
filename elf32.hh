#pragma once

#include "types.hh"

#include "func.hh"

#include <string_view>

void GetFunctionsFromElf(vec<Func> & out, std::string_view elf_name, vec<u8> const & elf_data, bool lax = true);

vec<Func> GetFunctionsFromElf(vec<u8> const & elf_data, bool lax = true);
