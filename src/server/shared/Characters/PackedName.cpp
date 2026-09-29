/*
 * Project Ambrose by Imjustchico
 * Writes and reads the packed name bit by bit, least significant first, as the client's bit stream does; a text of more bytes than five bits count cannot be packed, and a packed name is read back only when every field is present, its marker bits are set and nothing but padding follows, with an override read only as the UTF-16 text this server writes.
 */

#include "PackedName.h"
#include "Utf.h"

#include <utility>

namespace
{
    class BitWriter
    {
    public:
        void Write(uint32 value, uint32 bits)
        {
            for (uint32 bit = 0; bit < bits; ++bit, ++_used)
            {
                if (_used % 8 == 0)
                    _bytes.push_back('\0');
                if (((value >> bit) & 1u) != 0)
                    _bytes.back() = static_cast<char>(static_cast<uint8>(_bytes.back()) | static_cast<uint8>(1u << (_used % 8)));
            }
        }

        std::string Take()
        {
            return std::move(_bytes);
        }

    private:
        std::string _bytes;
        std::size_t _used = 0;
    };

    class BitReader
    {
    public:
        explicit BitReader(std::string_view bytes) : _bytes(bytes) { }

        std::optional<uint32> Read(uint32 bits)
        {
            if (_used + bits > _bytes.size() * 8)
                return std::nullopt;
            uint32 value = 0;
            for (uint32 bit = 0; bit < bits; ++bit, ++_used)
                if (((static_cast<uint8>(_bytes[_used / 8]) >> (_used % 8)) & 1u) != 0)
                    value |= 1u << bit;
            return value;
        }

        bool OnlyPaddingLeft() const noexcept
        {
            return (_used + 7) / 8 == _bytes.size();
        }

    private:
        std::string_view _bytes;
        std::size_t _used = 0;
    };
}

std::string PackedName::FromIndices(uint32 packedIndices, uint32 gender)
{
    NameIndices const indices = NameIndices::Unpack(packedIndices);
    BitWriter writer;
    writer.Write(0, 1);
    writer.Write(gender & 1u, 1);
    writer.Write(ReadersOwnLocale, 5);
    writer.Write(1, 1);
    writer.Write(indices.First, 8);
    writer.Write(indices.Middle, 8);
    writer.Write(indices.Last, 8);
    return writer.Take();
}

std::optional<std::string> PackedName::FromText(std::u16string_view text)
{
    std::size_t const bytes = text.size() * 2;
    if (text.empty() || bytes > MaxOverrideBytes)
        return std::nullopt;
    BitWriter writer;
    writer.Write(1, 1);
    writer.Write(0, 1);
    writer.Write(static_cast<uint32>(bytes), 5);
    writer.Write(1, 1);
    for (char16_t const unit : text)
    {
        writer.Write(static_cast<uint32>(unit) & 0xFFu, 8);
        writer.Write(static_cast<uint32>(unit) >> 8, 8);
    }
    return writer.Take();
}

std::string PackedName::ForWizard(std::optional<std::string> const& customName, uint32 packedIndices, uint32 gender)
{
    if (customName)
        if (std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(*customName, Utf::InvalidPolicy::Reject))
            if (std::optional<std::string> packed = FromText(*wide))
                return std::move(*packed);
    return FromIndices(packedIndices, gender);
}

std::optional<UnpackedName> PackedName::Unpack(std::string_view packed)
{
    BitReader reader(packed);
    std::optional<uint32> const overridden = reader.Read(1);
    if (!overridden)
        return std::nullopt;
    UnpackedName name;
    name.Override = *overridden != 0;
    if (!name.Override)
    {
        std::optional<uint32> const gender = reader.Read(1);
        std::optional<uint32> const locale = reader.Read(5);
        std::optional<uint32> const marker = reader.Read(1);
        std::optional<uint32> const first = reader.Read(8);
        std::optional<uint32> const middle = reader.Read(8);
        std::optional<uint32> const last = reader.Read(8);
        if (!gender || !locale || !marker || *marker != 1 || !first || !middle || !last || !reader.OnlyPaddingLeft())
            return std::nullopt;
        name.Gender = *gender;
        name.Indices = { static_cast<uint8>(*first), static_cast<uint8>(*middle), static_cast<uint8>(*last), static_cast<uint8>(*locale) };
        return name;
    }
    std::optional<uint32> const narrow = reader.Read(1);
    std::optional<uint32> const bytes = reader.Read(5);
    std::optional<uint32> const marker = reader.Read(1);
    if (!narrow || *narrow != 0 || !bytes || *bytes == 0 || *bytes % 2 != 0 || !marker || *marker != 1)
        return std::nullopt;
    for (uint32 unit = 0; unit < *bytes / 2; ++unit)
    {
        std::optional<uint32> const low = reader.Read(8);
        std::optional<uint32> const high = reader.Read(8);
        if (!low || !high)
            return std::nullopt;
        name.Text.push_back(static_cast<char16_t>(*low | (*high << 8)));
    }
    if (!reader.OnlyPaddingLeft())
        return std::nullopt;
    return name;
}
