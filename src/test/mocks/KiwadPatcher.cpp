/*
 * Project Ambrose by Imjustchico
 * Walks a KIWAD file's entry records to the one named, appends the new data deflated at the end of the file, and rewrites that record's offset, sizes, compressed flag and client CRC.
 */

#include "KiwadPatcher.h"
#include "Compression.h"
#include "Crc32.h"

#include <fmt/format.h>

#include <array>
#include <fstream>
#include <limits>

namespace
{
    bool ReadUInt32(std::fstream& file, uint32& value)
    {
        std::array<char, 4> bytes{};
        if (!file.read(bytes.data(), bytes.size()))
            return false;
        value = 0;
        for (int index = 3; index >= 0; --index)
            value = (value << 8) | static_cast<uint8>(bytes[index]);
        return true;
    }

    void WriteUInt32(std::fstream& file, uint32 value)
    {
        std::array<char, 4> bytes{};
        for (std::size_t index = 0; index < bytes.size(); ++index)
            bytes[index] = static_cast<char>(value >> (8 * index));
        file.write(bytes.data(), bytes.size());
    }
}

bool KiwadPatcher::Replace(std::filesystem::path const& archive, std::string_view name, std::vector<uint8> const& data, std::string& error)
{
    std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
    std::array<char, 5> magic{};
    uint32 version = 0;
    uint32 count = 0;
    if (!file || !file.read(magic.data(), magic.size()) || std::string_view(magic.data(), magic.size()) != "KIWAD" || !ReadUInt32(file, version) || !ReadUInt32(file, count))
    {
        error = fmt::format("{} is not a KIWAD file", archive.string());
        return false;
    }
    if (version >= 2)
        file.seekg(1, std::ios::cur);
    for (uint32 entry = 0; entry < count; ++entry)
    {
        std::streamoff const record = file.tellg();
        uint32 fields[5]{};
        uint8 compressed = 0;
        uint32 length = 0;
        if (!ReadUInt32(file, fields[0]) || !ReadUInt32(file, fields[1]) || !ReadUInt32(file, fields[2]) || !file.read(reinterpret_cast<char*>(&compressed), 1) ||
            !ReadUInt32(file, fields[3]) || !ReadUInt32(file, length))
        {
            error = fmt::format("{} ends inside entry record {}", archive.string(), entry);
            return false;
        }
        std::string entryName(length, '\0');
        if (!file.read(entryName.data(), length))
        {
            error = fmt::format("{} ends inside the name of entry record {}", archive.string(), entry);
            return false;
        }
        if (!entryName.empty() && entryName.back() == '\0')
            entryName.pop_back();
        if (entryName != name)
            continue;

        std::vector<uint8> const stored = Ambrose::Compression::Deflate(data);
        file.seekp(0, std::ios::end);
        std::streamoff const offset = file.tellp();
        if (offset + static_cast<std::streamoff>(stored.size()) > std::numeric_limits<uint32>::max())
        {
            error = fmt::format("{} would grow past what a KIWAD offset can name", archive.string());
            return false;
        }
        file.write(reinterpret_cast<char const*>(stored.data()), static_cast<std::streamsize>(stored.size()));
        file.seekp(record);
        WriteUInt32(file, static_cast<uint32>(offset));
        WriteUInt32(file, static_cast<uint32>(data.size()));
        WriteUInt32(file, static_cast<uint32>(stored.size()));
        uint8 const deflated = 1;
        file.write(reinterpret_cast<char const*>(&deflated), 1);
        WriteUInt32(file, Crc32::ComputeClient(stored));
        if (!file.flush())
        {
            error = fmt::format("{} could not be written", archive.string());
            return false;
        }
        return true;
    }
    error = fmt::format("{} holds no entry named {}", archive.string(), name);
    return false;
}
