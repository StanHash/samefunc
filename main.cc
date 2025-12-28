#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>

#include "types.hh"

#include "elf32.hh"

vec<u8> ReadEntireFile(std::string_view const & path)
{
    using namespace std;

    auto fclose_helper = [](FILE * f) { fclose(f); };
    auto file = unique_ptr<FILE, decltype(fclose_helper)>(fopen(path.data(), "rb"), fclose_helper);

    if (!file)
        return {};

    fseek(file.get(), 0, SEEK_END);
    vec<u8> result(ftell(file.get()));

    fseek(file.get(), 0, SEEK_SET);
    result.resize(fread(result.data(), 1, result.size(), file.get()));

    return result;
}

int main(int argc, char const * const * argv)
{
    vec<Func> funcs;

    bool compare_multiple_elves = argc > 2;

    for (int i = 1; i < argc; i++)
    {
        std::string_view elf_path(argv[i]);
        auto data = ReadEntireFile(elf_path);
        GetFunctionsFromElf(funcs, compare_multiple_elves ? elf_path : std::string_view {}, data, false);
    }

    for (Func const & func : funcs)
    {
        if (func.names.size() < 2)
            continue;

        if (compare_multiple_elves)
        {
            /* only print functions that are the same accross different elves
             * HACK: doing string compare on the parenthesized part of `func(elf)` */

            bool exists_in_different_elves = false;
            std::string_view first_elf(strchr(func.names[0].c_str(), '('));

            for (unsigned int i = 1; i < func.names.size(); i++)
            {
                std::string_view this_elf(strchr(func.names[i].c_str(), '('));

                if (first_elf != this_elf)
                {
                    exists_in_different_elves = true;
                }
            }

            if (!exists_in_different_elves)
                continue;
        }

        std::printf("%d", (int)func.data.size());
        for (auto const & name : func.names)
            std::printf(" %s", name.c_str());
        std::printf("\n");
    }
}
