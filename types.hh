#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef size_t usize;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef ptrdiff_t isize;

template <typename T> using vec = std::vector<T>;
using owned_str = std::string;
