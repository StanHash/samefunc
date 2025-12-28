#include "func.hh"

bool Func::Matches(bool is_thumb, u8 const * data, u8 const * mask, usize len) const
{
    if (this->is_thumb != is_thumb)
        return false;

    if (this->data.size() != len)
        return false;

    for (usize i = 0u; i < len; i++)
    {
        u8 mb = mask[i] & this->mask[i];
        u8 lb = this->data[i] & mb;
        u8 rb = data[i] & mb;

        if (lb != rb)
            return false;
    }

    return true;
}
