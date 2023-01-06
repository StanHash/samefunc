#include <cstdio>
#include <memory>
#include <string_view>

#include "types.hh"

#include "elf32.hh"

vec<u8> ReadEntireFile(std::string_view const & path)
{
    using namespace std;

    auto fclose_helper = [](FILE * f) { fclose(f); };
    auto file = unique_ptr<FILE, decltype(fclose_helper)>(
        fopen(path.data(), "rb"), fclose_helper);

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

    for (int i = 1; i < argc; i++)
    {
        std::string_view elf_path(argv[i]);
        auto data = ReadEntireFile(elf_path);
        GetFunctionsFromElf(funcs, data);
    }

    for (Func const & func : funcs)
    {
        if (func.names.size() < 2)
            continue;

        std::printf("%d", (int)func.data.size());
        for (auto const & name : func.names)
            std::printf(" %s", name.c_str());
        std::printf("\n");
    }
}
