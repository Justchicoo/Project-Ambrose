/*
 * Project Ambrose by Imjustchico
 * Serializes validated type-dump records into a bounded fixed-width cache with one shared string table and an exact payload hash.
 */

#include "TypeRegistryBinary.h"

#include "Hex.h"
#include "SHA256.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <span>
#include <unordered_map>

namespace
{
    constexpr std::array<char, 8> Magic{ 'A', 'M', 'B', 'T', 'R', 'E', 'G', '1' };
    constexpr uint32 Version = 2;
    constexpr std::uintmax_t MaxPayloadBytes = std::uintmax_t{ 512 } << 20;
    constexpr uint32 Missing = std::numeric_limits<uint32>::max();
    constexpr uint16 Type = 1u << 0;
    constexpr uint16 Container = 1u << 1;
    constexpr uint16 Id = 1u << 2;
    constexpr uint16 Offset = 1u << 3;
    constexpr uint16 Flags = 1u << 4;
    constexpr uint16 Hash = 1u << 5;
    constexpr uint16 Dynamic = 1u << 6;
    constexpr uint16 Singleton = 1u << 7;
    constexpr uint16 Pointer = 1u << 8;

    void PutU8(std::string& bytes, uint8 value) { bytes.push_back(static_cast<char>(value)); }
    void PutU16(std::string& bytes, uint16 value)
    {
        bytes.push_back(static_cast<char>(value & 0xFFu));
        bytes.push_back(static_cast<char>(value >> 8));
    }
    void PutU32(std::string& bytes, uint32 value)
    {
        for (int shift = 0; shift != 32; shift += 8)
            bytes.push_back(static_cast<char>((value >> shift) & 0xFFu));
    }
    void PutU64(std::string& bytes, uint64 value)
    {
        for (int shift = 0; shift != 64; shift += 8)
            bytes.push_back(static_cast<char>((value >> shift) & 0xFFu));
    }

    bool Get(std::string_view bytes, std::size_t& offset, std::size_t count, void* output)
    {
        if (count > bytes.size() - offset)
            return false;
        std::memcpy(output, bytes.data() + offset, count);
        offset += count;
        return true;
    }
    bool GetString(std::string_view bytes, std::size_t& offset, std::size_t count, std::string& output)
    {
        if (count > bytes.size() - offset)
            return false;
        output.assign(bytes.substr(offset, count));
        offset += count;
        return true;
    }
    bool GetU8(std::string_view bytes, std::size_t& offset, uint8& value) { return Get(bytes, offset, 1, &value); }
    bool GetU16(std::string_view bytes, std::size_t& offset, uint16& value)
    {
        uint8 raw[2];
        if (!Get(bytes, offset, sizeof(raw), raw))
            return false;
        value = uint16(raw[0]) | (uint16(raw[1]) << 8);
        return true;
    }
    bool GetU32(std::string_view bytes, std::size_t& offset, uint32& value)
    {
        uint8 raw[4];
        if (!Get(bytes, offset, sizeof(raw), raw))
            return false;
        value = uint32(raw[0]) | (uint32(raw[1]) << 8) | (uint32(raw[2]) << 16) | (uint32(raw[3]) << 24);
        return true;
    }
    bool GetU64(std::string_view bytes, std::size_t& offset, uint64& value)
    {
        uint8 raw[8];
        if (!Get(bytes, offset, sizeof(raw), raw))
            return false;
        value = 0;
        for (int shift = 0; shift != 64; shift += 8)
            value |= uint64(raw[shift / 8]) << shift;
        return true;
    }

    struct Strings
    {
        std::vector<std::string> Values;
        std::unordered_map<std::string, uint32> Offsets;
        uint32 Size = 0;

        uint32 Add(std::string_view value)
        {
            auto const found = Offsets.find(std::string(value));
            if (found != Offsets.end())
                return found->second;
            uint32 const offset = Size;
            Values.emplace_back(value);
            Offsets.emplace(Values.back(), offset);
            Size += static_cast<uint32>(value.size() + 1);
            return offset;
        }
    };

    void AddStrings(TypeDumpLoader::RawDump const& dump, Strings& strings)
    {
        for (auto const& entry : dump.Classes)
        {
            strings.Add(entry.Key);
            if (entry.Name)
                strings.Add(*entry.Name);
            for (std::string const& base : entry.Bases)
                strings.Add(base);
            for (auto const& property : entry.Properties)
            {
                strings.Add(property.Name);
                if (property.Type)
                    strings.Add(*property.Type);
                if (property.Container)
                    strings.Add(*property.Container);
                for (auto const& option : property.Options)
                {
                    strings.Add(option.first);
                    if (std::holds_alternative<std::string>(option.second))
                        strings.Add(std::get<std::string>(option.second));
                }
            }
        }
    }

    uint32 StringOffset(Strings const& strings, std::string_view value)
    {
        return strings.Offsets.at(std::string(value));
    }

    std::string BuildPayload(TypeDumpLoader::RawDump const& dump)
    {
        Strings strings;
        AddStrings(dump, strings);
        uint32 const classCount = static_cast<uint32>(dump.Classes.size());
        uint32 propertyCount = 0;
        uint32 baseCount = 0;
        uint32 optionCount = 0;
        for (auto const& entry : dump.Classes)
        {
            propertyCount += static_cast<uint32>(entry.Properties.size());
            baseCount += static_cast<uint32>(entry.Bases.size());
            for (auto const& property : entry.Properties)
                optionCount += static_cast<uint32>(property.Options.size());
        }

        std::string table;
        for (std::string const& value : strings.Values)
            table.append(value).push_back('\0');

        std::string payload;
        PutU32(payload, 1);
        PutU32(payload, static_cast<uint32>(table.size()));
        PutU32(payload, classCount);
        PutU32(payload, propertyCount);
        PutU32(payload, baseCount);
        PutU32(payload, optionCount);
        PutU32(payload, dump.Version ? static_cast<uint32>(*dump.Version) : Missing);
        for (auto const& entry : dump.Classes)
        {
            PutU32(payload, StringOffset(strings, entry.Key));
            PutU32(payload, entry.Name ? StringOffset(strings, *entry.Name) : Missing);
            PutU32(payload, entry.Hash ? static_cast<uint32>(*entry.Hash) : Missing);
            PutU32(payload, static_cast<uint32>(entry.Bases.size()));
            PutU32(payload, static_cast<uint32>(entry.Properties.size()));
            for (std::string const& base : entry.Bases)
                PutU32(payload, StringOffset(strings, base));
            for (auto const& property : entry.Properties)
            {
                uint16 mask = 0;
                mask |= property.Type ? Type : 0;
                mask |= property.Container ? Container : 0;
                mask |= property.Id ? Id : 0;
                mask |= property.Offset ? Offset : 0;
                mask |= property.Flags ? Flags : 0;
                mask |= property.Hash ? Hash : 0;
                mask |= property.Dynamic ? Dynamic : 0;
                mask |= property.Singleton ? Singleton : 0;
                mask |= property.Pointer ? Pointer : 0;
                PutU32(payload, StringOffset(strings, property.Name));
                PutU16(payload, mask);
                PutU32(payload, property.Type ? StringOffset(strings, *property.Type) : Missing);
                PutU32(payload, property.Container ? StringOffset(strings, *property.Container) : Missing);
                PutU64(payload, property.Id.value_or(0));
                PutU64(payload, property.Offset.value_or(0));
                PutU64(payload, property.Flags.value_or(0));
                PutU64(payload, property.Hash.value_or(0));
                PutU8(payload, property.Dynamic.value_or(false) ? 1 : 0);
                PutU8(payload, property.Singleton.value_or(false) ? 1 : 0);
                PutU8(payload, property.Pointer.value_or(false) ? 1 : 0);
                PutU32(payload, static_cast<uint32>(property.Options.size()));
                for (auto const& option : property.Options)
                {
                    PutU32(payload, StringOffset(strings, option.first));
                    PutU8(payload, std::holds_alternative<int64>(option.second) ? 0 : 1);
                    if (std::holds_alternative<int64>(option.second))
                        PutU64(payload, static_cast<uint64>(std::get<int64>(option.second)));
                    else
                        PutU32(payload, StringOffset(strings, std::get<std::string>(option.second)));
                }
            }
        }
        payload.append(table);
        return payload;
    }

    bool ReadString(std::string_view table, uint32 offset, std::string& value)
    {
        if (offset == Missing || offset >= table.size())
            return false;
        std::size_t const end = table.find('\0', offset);
        if (end == std::string_view::npos)
            return false;
        value.assign(table.substr(offset, end - offset));
        return true;
    }
}

bool TypeRegistryBinary::Write(std::filesystem::path const& output, TypeDumpLoader::RawDump const& dump, std::string_view revision, std::string& error)
{
    std::string const payload = BuildPayload(dump);
    if (payload.size() > MaxPayloadBytes || revision.size() > std::numeric_limits<uint32>::max())
    {
        error = "the type registry is too large for the binary cache";
        return false;
    }
    std::string const hash = Hex::Encode(SHA256::GetDigestOf(std::span<uint8 const>(reinterpret_cast<uint8 const*>(payload.data()), payload.size())));
    std::string bytes;
    bytes.append(Magic.data(), Magic.size());
    PutU32(bytes, Version);
    PutU32(bytes, static_cast<uint32>(revision.size()));
    PutU64(bytes, payload.size());
    PutU32(bytes, static_cast<uint32>(hash.size()));
    bytes.append(revision);
    bytes.append(hash);
    bytes.append(payload);
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
    {
        error = "cannot write " + output.string();
        return false;
    }
    return true;
}

bool TypeRegistryBinary::Read(std::filesystem::path const& input, std::string_view expectedRevision, TypeDumpLoader::RawDump& dump, std::string& revision, std::string& payloadHash, std::string& error)
{
    std::error_code fileError;
    std::uintmax_t const size = std::filesystem::file_size(input, fileError);
    if (fileError || size > MaxPayloadBytes + 128)
    {
        error = fileError ? "cannot stat " + input.string() + ": " + fileError.message() : "the binary type-registry cache is too large";
        return false;
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    std::ifstream stream(input, std::ios::binary);
    if (!stream || !stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
    {
        error = "cannot read " + input.string();
        return false;
    }
    std::size_t offset = 0;
    if (bytes.size() < Magic.size() || !std::equal(Magic.begin(), Magic.end(), bytes.begin()))
    {
        error = "the binary type-registry cache has an invalid magic";
        return false;
    }
    offset += Magic.size();
    uint32 version = 0;
    uint32 revisionLength = 0;
    uint64 payloadLength = 0;
    uint32 hashLength = 0;
    std::string storedHash;
    std::string payload;
    if (!GetU32(bytes, offset, version) || !GetU32(bytes, offset, revisionLength) || !GetU64(bytes, offset, payloadLength) || !GetU32(bytes, offset, hashLength) ||
    version != Version || hashLength != 64 || !GetString(bytes, offset, revisionLength, revision) || !GetString(bytes, offset, hashLength, storedHash) ||
    payloadLength > bytes.size() - offset || !GetString(bytes, offset, static_cast<std::size_t>(payloadLength), payload) || offset != bytes.size())
    {
        error = "the binary type-registry cache is truncated or has an unsupported header";
        return false;
    }
    if (!expectedRevision.empty() && revision != expectedRevision)
    {
        error = "the binary type-registry cache is for revision " + revision + ", expected " + std::string(expectedRevision);
        return false;
    }
    payloadHash = Hex::Encode(SHA256::GetDigestOf(std::span<uint8 const>(reinterpret_cast<uint8 const*>(payload.data()), payload.size())));
    if (storedHash != payloadHash)
    {
        error = "the binary type-registry cache SHA-256 does not match its payload";
        return false;
    }

    std::size_t cursor = 0;
    uint32 payloadVersion = 0, tableBytes = 0, classCount = 0, propertyCount = 0, baseCount = 0, optionCount = 0, dumpVersion = 0;
    if (!GetU32(payload, cursor, payloadVersion) || !GetU32(payload, cursor, tableBytes) || !GetU32(payload, cursor, classCount) || !GetU32(payload, cursor, propertyCount) ||
        !GetU32(payload, cursor, baseCount) || !GetU32(payload, cursor, optionCount) || !GetU32(payload, cursor, dumpVersion) || payloadVersion != 1)
    {
        error = "the binary type-registry payload has an unsupported header";
        return false;
    }
    for (uint32 i = 0; i < classCount; ++i)
    {
        uint32 key = 0, name = 0, hash = 0, bases = 0, properties = 0;
        if (!GetU32(payload, cursor, key) || !GetU32(payload, cursor, name) || !GetU32(payload, cursor, hash) ||
            !GetU32(payload, cursor, bases) || !GetU32(payload, cursor, properties))
            return false;
        for (uint32 base = 0; base < bases; ++base)
            if (!GetU32(payload, cursor, key))
                return false;
        for (uint32 p = 0; p < properties; ++p)
        {
            uint16 mask = 0;
            uint64 value = 0;
            uint32 options = 0;
            if (!GetU32(payload, cursor, key) || !GetU16(payload, cursor, mask) || !GetU32(payload, cursor, key) || !GetU32(payload, cursor, key))
                return false;
            for (int field = 0; field != 4; ++field)
                if (!GetU64(payload, cursor, value))
                    return false;
            uint8 flag = 0;
            if (!GetU8(payload, cursor, flag) || !GetU8(payload, cursor, flag) || !GetU8(payload, cursor, flag) || !GetU32(payload, cursor, options))
                return false;
            for (uint32 option = 0; option < options; ++option)
            {
                if (!GetU32(payload, cursor, key) || !GetU8(payload, cursor, flag))
                    return false;
                if (flag == 0)
                {
                    if (!GetU64(payload, cursor, value))
                        return false;
                }
                else if (!GetU32(payload, cursor, key))
                    return false;
            }
        }
    }
    if (cursor > payload.size() || tableBytes > payload.size() - cursor)
    {
        error = "the binary type-registry payload has an invalid string table";
        return false;
    }
    std::string_view const table(payload.data() + cursor, tableBytes);
    dump = {};
    if (dumpVersion != Missing)
        dump.Version = dumpVersion;
    dump.HasClasses = true;
    cursor = 28;
    for (uint32 i = 0; i < classCount; ++i)
    {
        uint32 key = 0, name = 0, hash = 0, bases = 0, properties = 0;
        GetU32(payload, cursor, key); GetU32(payload, cursor, name); GetU32(payload, cursor, hash); GetU32(payload, cursor, bases); GetU32(payload, cursor, properties);
        auto& entry = dump.Classes.emplace_back();
        if (!ReadString(table, key, entry.Key) || (name != Missing && !ReadString(table, name, entry.Name.emplace())))
        {
            error = "the binary type-registry payload has an invalid string offset";
            return false;
        }
        if (hash != Missing)
            entry.Hash = hash;
        for (uint32 b = 0; b < bases; ++b)
        {
            uint32 base = 0;
            GetU32(payload, cursor, base);
            entry.Bases.emplace_back();
            if (!ReadString(table, base, entry.Bases.back()))
                return false;
        }
        for (uint32 p = 0; p < properties; ++p)
        {
            auto& property = entry.Properties.emplace_back();
            uint32 propertyName = 0, type = 0, container = 0, options = 0;
            uint16 mask = 0;
            uint64 id = 0, offsetValue = 0, flags = 0, propertyHash = 0;
            GetU32(payload, cursor, propertyName); GetU16(payload, cursor, mask); GetU32(payload, cursor, type); GetU32(payload, cursor, container);
            GetU64(payload, cursor, id); GetU64(payload, cursor, offsetValue); GetU64(payload, cursor, flags); GetU64(payload, cursor, propertyHash);
            uint8 dynamic = 0, singleton = 0, pointer = 0;
            GetU8(payload, cursor, dynamic); GetU8(payload, cursor, singleton); GetU8(payload, cursor, pointer); GetU32(payload, cursor, options);
            if (!ReadString(table, propertyName, property.Name) || (mask & Type && !ReadString(table, type, property.Type.emplace())) ||
                (mask & Container && !ReadString(table, container, property.Container.emplace())))
                return false;
            if (mask & Id) property.Id = id;
            if (mask & Offset) property.Offset = offsetValue;
            if (mask & Flags) property.Flags = flags;
            if (mask & Hash) property.Hash = propertyHash;
            if (mask & Dynamic) property.Dynamic = dynamic != 0;
            if (mask & Singleton) property.Singleton = singleton != 0;
            if (mask & Pointer) property.Pointer = pointer != 0;
            for (uint32 o = 0; o < options; ++o)
            {
                uint32 optionName = 0, optionText = 0;
                uint8 kind = 0;
                GetU32(payload, cursor, optionName); GetU8(payload, cursor, kind);
                if (!ReadString(table, optionName, property.Options.emplace_back().first))
                    return false;
                if (kind == 0)
                {
                    uint64 value = 0;
                    GetU64(payload, cursor, value);
                    property.Options.back().second = static_cast<int64>(value);
                }
                else
                {
                    GetU32(payload, cursor, optionText);
                    std::string text;
                    if (!ReadString(table, optionText, text))
                        return false;
                    property.Options.back().second = std::move(text);
                }
            }
        }
    }
    return true;
}
