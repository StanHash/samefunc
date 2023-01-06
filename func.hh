#pragma once

#include "types.hh"

struct Func
{
    Func(
        bool is_thumb, char const * name, u8 const * data, u8 const * mask,
        usize len)
        : is_thumb(is_thumb), data(data, data + len), mask(mask, mask + len)
    {
        AddName(name);
    }

    Func(Func const &) = default;
    Func(Func &&) = default;

    Func & operator=(Func const &) = default;
    Func & operator=(Func &&) = default;

    void AddName(char const * name) { names.emplace_back(name); }

    bool
    Matches(bool is_thumb, u8 const * data, u8 const * mask, usize len) const;

    bool Matches(Func const & other) const
    {
        return Matches(
            other.is_thumb, other.data.data(), other.mask.data(),
            other.data.size());
    }

    static bool Matches(Func const & left, Func const & right)
    {
        return left.Matches(right);
    }

    bool is_thumb;
    vec<owned_str> names;
    vec<u8> data;
    vec<u8> mask;
};
