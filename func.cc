#include "func.hh"

bool Func::Matches(
    bool is_thumb, u8 const * data, u8 const * mask, usize len) const
{
    if (this->is_thumb != is_thumb)
        return false;

    if (this->data.size() != len)
        return false;

    for (usize i = 0u; i < len; i++)
    {
        auto lb = this->data[i] & this->mask[i];
        auto rb = data[i] & mask[i];

        if (lb != rb)
            return false;
    }

    return true;
}
