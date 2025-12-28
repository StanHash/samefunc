#include "elf32.hh"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>

#include <elf.h>

// read little endian int of any size
template <typename T> static T leswap(T const & src)
{
    auto bytes = reinterpret_cast<u8 const *>(&src);

    T result = 0;

    for (usize i = 0; i < sizeof(T); i++)
        result |= bytes[i] << (i * 8);

    return result;
}

struct MemPt
{
    enum Kind
    {
        Unknown,
        Arm,
        Thumb,
        Data,
    };

    usize offset = 0;
    Kind kind = Unknown;
};

void MaskMem(MemPt::Kind kind, u8 const * data, u8 * mask, usize len)
{
    if (kind != MemPt::Thumb)
        // we don't know how to mask anything other than thumb
        return;

    // THUMB ONLY

    for (usize off = 0; off < len; off += 2)
    {
        u16 ins = data[off] + (data[off + 1] << 8);

        if ((ins & 0xFC00) == 0x1C00)
        {
            // immediate of add/sub r3, r3, n3
            mask[off + 1] = 0xFE;
            mask[off + 0] = 0x1F;
            continue;
        }

        if ((ins & 0xE000) == 0x2000)
        {
            // immediate of mov/cmp/add/sub r3, n8
            mask[off] = 0x00;
            continue;
        }

        if ((ins & 0xE000) == 0x6000)
        {
            // ldr/str/ldrb/strb r3, [r3, n5] offset
            mask[off + 1] = 0xF8;
            mask[off + 0] = 0x3F;
            continue;
        }

        if ((ins & 0xF000) == 0x8000)
        {
            // ldrh/strh r3, [r3, n5] offset
            mask[off + 1] = 0xF8;
            mask[off + 0] = 0x3F;
            continue;
        }
    }
}

void GetFunctionsFromElf(vec<Func> & out, std::string_view elf_name, vec<u8> const & elf_data, bool lax)
{
    auto head = reinterpret_cast<Elf32_Ehdr const *>(elf_data.data());

    // TODO: do not assert and do good error instead

    assert(head->e_ident[EI_MAG0] == ELFMAG0);
    assert(head->e_ident[EI_MAG1] == ELFMAG1);
    assert(head->e_ident[EI_MAG2] == ELFMAG2);
    assert(head->e_ident[EI_MAG3] == ELFMAG3);
    assert(head->e_ident[EI_CLASS] == ELFCLASS32);
    assert(head->e_ident[EI_DATA] == ELFDATA2LSB);
    assert(leswap(head->e_machine) == EM_ARM);

    bool is_relocatable_elf = leswap(head->e_type) == ET_REL;

    auto shnum = leswap(head->e_shnum);
    auto shoff = leswap(head->e_shoff);
    auto shent = leswap(head->e_shentsize);

    // helper lambda
    auto sec = [shnum, shoff, shent, &elf_data](usize idx)
    { return reinterpret_cast<Elf32_Shdr const *>(elf_data.data() + shoff + shent * idx); };

    vec<u8> sec_data;
    vec<usize> sec_mask_off(shnum, SIZE_MAX);

    vec<u32> sec_vaddr(shnum, UINT32_MAX);

    vec<MemPt> mem_points;

    // get section data

    for (usize i = 0; i < shnum; i++)
    {
        auto shdr = sec(i);

        sec_vaddr[i] = shdr->sh_addr;

        if (leswap(shdr->sh_type) == SHT_PROGBITS)
        {
            auto size = leswap(shdr->sh_size);
            auto data = elf_data.data() + leswap(shdr->sh_offset);

            sec_mask_off[i] = sec_data.size();
            mem_points.push_back({ sec_data.size(), MemPt::Unknown });
            std::copy_n(data, size, std::back_inserter(sec_data));
        }
    }

    // end point
    mem_points.push_back({ sec_data.size(), MemPt::Unknown });

    // set detailed memory points

    for (usize i = 0; i < shnum; i++)
    {
        auto shdr = sec(i);

        if (leswap(shdr->sh_type) == SHT_SYMTAB)
        {
            auto name_data = elf_data.data() + leswap(sec(leswap(shdr->sh_link))->sh_offset);

            auto entsize = leswap(shdr->sh_entsize);
            auto entcount = leswap(shdr->sh_info); // local sym end
            auto sym_data = elf_data.data() + leswap(shdr->sh_offset);

            for (usize j = 0; j < entcount; j++)
            {
                auto sym = reinterpret_cast<Elf32_Sym const *>(sym_data + entsize * j);

                if (ELF32_ST_BIND(leswap(sym->st_info)) != STB_LOCAL)
                    continue;

                if (leswap(sym->st_shndx) >= shnum)
                    // not in a section
                    continue;

                auto data_offset = sec_mask_off[leswap(sym->st_shndx)];

                if (data_offset > sec_data.size())
                    // not in a section with data
                    continue;

                auto name = reinterpret_cast<char const *>(name_data + leswap(sym->st_name));

                if (std::strlen(name) != 2 || name[0] != '$')
                    continue;

                auto kind = name[1] == 't'   ? MemPt::Thumb
                            : name[1] == 'a' ? MemPt::Arm
                            : name[1] == 'd' ? MemPt::Data
                                             : MemPt::Unknown;

                auto value = leswap(sym->st_value);

                if (!is_relocatable_elf)
                {
                    value = value - sec_vaddr[sym->st_shndx];
                }

                auto point_offset = data_offset + (value & ~1);

                assert(point_offset < sec_data.size());

                mem_points.push_back({ point_offset, kind });
            }
        }
    }

    // sort memory points (allows binary search later)
    std::sort(
        mem_points.begin(), mem_points.end(), [](auto const & left, auto const & right)
        { return left.offset * 4 + (int)left.kind < right.offset * 4 + (int)right.kind; });

    // make section masks

    vec<u8> sec_mask(sec_data.size(), 0xFF);

    // from rels

    for (usize i = 0; i < shnum; i++)
    {
        auto shdr = sec(i);

        // making sure we can use the same code for both rel and rela
        static_assert(offsetof(Elf32_Rel, r_offset) == offsetof(Elf32_Rela, r_offset));
        static_assert(offsetof(Elf32_Rel, r_info) == offsetof(Elf32_Rela, r_info));

        auto sh_type = leswap(shdr->sh_type);

        if (sh_type == SHT_REL || sh_type == SHT_RELA)
        {
            auto applies_to = leswap(shdr->sh_info);

            if (sec_mask_off[applies_to] > sec_data.size())
                continue;

            auto mask_data = sec_mask.data() + sec_mask_off[applies_to];

            auto entsize = leswap(shdr->sh_entsize);
            auto entcount = leswap(shdr->sh_size) / entsize;
            auto rel_data = elf_data.data() + leswap(shdr->sh_offset);

            for (usize j = 0; j < entcount; j++)
            {
                auto rel = reinterpret_cast<Elf32_Rel const *>(rel_data + entsize * j);

                auto offset = leswap(rel->r_offset);

                switch (ELF32_R_TYPE(leswap(rel->r_info)))
                {
                    case R_ARM_ABS32:
                    case R_ARM_REL32:
                        std::fill_n(mask_data + offset, 4, 0x00);
                        break;

                    case R_ARM_ABS16:
                        std::fill_n(mask_data + offset, 2, 0x00);
                        break;

                    case R_ARM_ABS8:
                        std::fill_n(mask_data + offset, 1, 0x00);
                        break;

                    case R_ARM_THM_PC22: // R_ARM_THM_CALL
                        mask_data[offset + 0] = 0x00;
                        mask_data[offset + 1] = 0xF8;
                        mask_data[offset + 2] = 0x00;
                        mask_data[offset + 3] = 0xF8;
                        break;
                }
            }
        }
    }

    // from asm scan (using memory points to know where asm is)

    if (lax)
    {
        for (usize i = 0; i < mem_points.size() - 1; i++)
        {
            auto const & pt = mem_points[i];

            if (pt.kind == MemPt::Unknown)
                continue;

            auto const & pt_next = mem_points[i + 1];

            MaskMem(pt.kind, sec_data.data() + pt.offset, sec_mask.data() + pt.offset, pt_next.offset - pt.offset);
        }
    }

    // extract functions

    for (usize i = 0; i < shnum; i++)
    {
        auto shdr = sec(i);

        if (leswap(shdr->sh_type == SHT_SYMTAB))
        {
            auto name_data = elf_data.data() + leswap(sec(leswap(shdr->sh_link))->sh_offset);

            auto entsize = leswap(shdr->sh_entsize);
            auto entcount = leswap(shdr->sh_size) / entsize;
            auto sym_data = elf_data.data() + leswap(shdr->sh_offset);

            for (usize j = 0; j < entcount; j++)
            {
                auto sym = reinterpret_cast<Elf32_Sym const *>(sym_data + entsize * j);

                if (ELF32_ST_TYPE(leswap(sym->st_info)) != STT_FUNC)
                    // not a function
                    continue;

                auto size = leswap(sym->st_size);

                if (size == 0)
                    // not a sized function (cannot know data)
                    continue;

                if (leswap(sym->st_shndx) >= shnum)
                    // not in a section
                    continue;

                auto data_offset = sec_mask_off[leswap(sym->st_shndx)];

                if (data_offset > sec_data.size())
                    // not in a section with data
                    continue;

                auto value = leswap(sym->st_value);

                if (!is_relocatable_elf)
                {
                    value = value - sec_vaddr[sym->st_shndx];
                }

                auto is_thumb = (value & 1) != 0;

                auto func_offset = data_offset + (value & ~1);

                assert(func_offset < sec_data.size());

                auto name = reinterpret_cast<char const *>(name_data + leswap(sym->st_name));

                auto data = sec_data.data() + func_offset;
                auto mask = sec_mask.data() + func_offset;

                /* HACK: align size to next 4-byte boundry */
                size = size + ((4u - size) % 4u);

                bool found = false;

                auto name_q = elf_name.empty() ? std::string(name) : std::format("{1}({0})", elf_name, name);

                for (Func & func : out)
                {
                    if (func.Matches(is_thumb, data, mask, size))
                    {
                        func.AddName(std::move(name_q));
                        found = true;
                        break;
                    }
                }

                if (!found)
                    out.emplace_back(is_thumb, std::move(name_q), data, mask, size);
            }
        }
    }
}

vec<Func> GetFunctionsFromElf(vec<u8> const & elf_data, bool lax)
{
    vec<Func> result;
    GetFunctionsFromElf(result, {}, elf_data, lax);
    return result;
}
