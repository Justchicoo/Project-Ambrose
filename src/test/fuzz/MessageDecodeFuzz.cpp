/*
 * Project Ambrose by Imjustchico
 * Exercises dynamic DML message decoding, including bounded STR and WSTR fields, with arbitrary frame bodies.
 */

#include "AllocationCeiling.h"
#include "DynamicMessage.h"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
    constexpr std::string_view Definition = R"(<MessageFuzz><_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">9</ServiceID><ProtocolType TYPE="STR">DYNAMIC</ProtocolType></RECORD></_ProtocolInfo><MSG_FUZZ><RECORD><Count TYPE="UINT"></Count><Text TYPE="STR"></Text><Wide TYPE="WSTR"></Wide></RECORD></MSG_FUZZ></MessageFuzz>)";
    MessageRegistry Registry;
    MessageCatalogPtr Catalog;
    MessageInfo const* Info = nullptr;

    void WriteSeeds(std::filesystem::path const& folder)
    {
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error)
            std::abort();

        DynamicMessage message(Catalog, *Info);
        if (!message.Set("Text", DmlValue(std::string("abc"))) || !message.Set("Wide", DmlValue(std::u16string(u"xyz"))))
            std::abort();
        ByteBuffer body;
        message.Encode(body);
        std::ofstream stream(folder / "valid-string-fields.bin", std::ios::binary | std::ios::trunc);
        if (!stream)
            std::abort();
        stream.write(reinterpret_cast<char const*>(body.GetData().data()), static_cast<std::streamsize>(body.GetSize()));
        if (!stream)
            std::abort();
    }
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    AllocationCeiling::Install();
    MessageDefinitionSet definitions;
    if (!definitions.Add(Definition, "MessageFuzz.xml") || !Registry.Load(std::move(definitions)))
        std::abort();
    Catalog = Registry.GetCatalog();
    Info = Catalog->Find(9, "MSG_FUZZ");
    if (!Info)
        std::abort();

    for (int index = 1; index < *argc; ++index)
    {
        std::string_view const argument = (*argv)[index];
        if (!argument.empty() && argument.front() != '-')
        {
            WriteSeeds(std::filesystem::path(argument));
            break;
        }
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, std::size_t size)
{
    AllocationCeiling::Scope const ceiling;
    DynamicMessage message(Catalog, *Info);
    message.Decode(std::span<uint8 const>(data, size));
    return 0;
}
