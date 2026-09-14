/*
 * Project Ambrose by Imjustchico
 * Generates splitmix64-driven values for every DML type and reports the first round-trip property a message breaks.
 */

#include "MessageRoundTrip.h"

#include <fmt/format.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace
{
    class SplitMix64
    {
    public:
        explicit SplitMix64(uint64 seed) noexcept : _state(seed)
        {
        }

        uint64 Next() noexcept
        {
            uint64 value = (_state += 0x9E3779B97F4A7C15ull);
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

    private:
        uint64 _state;
    };

    DmlValue RandomValue(DmlType type, SplitMix64& random)
    {
        uint64 const bits = random.Next();
        switch (type)
        {
            case DmlType::Byt: return static_cast<int8>(static_cast<uint8>(bits));
            case DmlType::Ubyt: return static_cast<uint8>(bits);
            case DmlType::Shrt: return static_cast<int16>(static_cast<uint16>(bits));
            case DmlType::Ushrt: return static_cast<uint16>(bits);
            case DmlType::Int: return static_cast<int32>(static_cast<uint32>(bits));
            case DmlType::Uint: return static_cast<uint32>(bits);
            case DmlType::Flt:
            {
                float value = std::bit_cast<float>(static_cast<uint32>(bits));
                return std::isfinite(value) ? value : std::bit_cast<float>(static_cast<uint32>(bits) & 0x807FFFFFu);
            }
            case DmlType::Dbl:
            {
                double value = std::bit_cast<double>(bits);
                return std::isfinite(value) ? value : std::bit_cast<double>(bits & 0x800FFFFFFFFFFFFFull);
            }
            case DmlType::Gid: return bits;
            case DmlType::Str:
            {
                std::string text(static_cast<std::size_t>(bits % 24), '\0');
                for (char& character : text)
                    character = static_cast<char>(static_cast<uint8>(random.Next()));
                return text;
            }
            case DmlType::Wstr:
            {
                std::u16string text(static_cast<std::size_t>(bits % 24), u'\0');
                for (char16_t& unit : text)
                    unit = static_cast<char16_t>(static_cast<uint16>(random.Next()));
                return text;
            }
        }
        return uint8(0);
    }
}

uint64 MessageRoundTrip::SeedFor(MessageInfo const& info) noexcept
{
    return (uint64{ info.Protocol->ServiceId } << 8 | info.Definition->Order) ^ 0xA5B35705C0FFEEull;
}

DynamicMessage MessageRoundTrip::MakeRandom(MessageInfo const& info, uint64 seed)
{
    SplitMix64 random(seed);
    DynamicMessage message(info);
    std::vector<FieldDef> const& fields = info.Definition->Fields;
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        if (!message.Set(i, RandomValue(fields[i].Type, random)))
            throw std::logic_error(fmt::format("{}.{} rejected a generated {} value", info.Definition->Tag, fields[i].Name, Dml::GetTypeName(fields[i].Type)));
    }
    return message;
}

bool MessageRoundTrip::SameValue(DmlValue const& left, DmlValue const& right) noexcept
{
    if (left.index() != right.index())
        return false;
    if (float const* const value = std::get_if<float>(&left))
        return std::bit_cast<uint32>(*value) == std::bit_cast<uint32>(std::get<float>(right));
    if (double const* const value = std::get_if<double>(&left))
        return std::bit_cast<uint64>(*value) == std::bit_cast<uint64>(std::get<double>(right));
    return left == right;
}

std::string MessageRoundTrip::Check(MessageInfo const& info, uint64 seed)
{
    MessageDef const& definition = *info.Definition;
    std::string const name = fmt::format("{} ({}:{})", definition.Tag, info.Protocol->ServiceId, definition.Order);

    DynamicMessage const defaults(info);
    if (defaults.GetEncodedSize() < info.MinSize)
        return fmt::format("{}: default size {} is below the minimum size {}", name, defaults.GetEncodedSize(), info.MinSize);

    DynamicMessage const original = MakeRandom(info, seed);
    ByteBuffer buffer;
    original.Encode(buffer);
    std::vector<uint8> const bytes(buffer.GetData().begin(), buffer.GetData().end());
    if (bytes.size() != original.GetEncodedSize())
        return fmt::format("{}: encoded {} bytes but the values need {}", name, bytes.size(), original.GetEncodedSize());
    if (bytes.size() < info.MinSize)
        return fmt::format("{}: encoded {} bytes, below the minimum size {}", name, bytes.size(), info.MinSize);

    DynamicMessage decoded(info);
    MessageDecodeStatus const status = decoded.Decode(bytes);
    if (status != MessageDecodeStatus::Ok)
        return fmt::format("{}: decode returned status {}", name, static_cast<int32>(status));
    for (std::size_t i = 0; i < definition.Fields.size(); ++i)
    {
        if (!SameValue(original.GetValues()[i], decoded.GetValues()[i]))
            return fmt::format("{}: field {} changed from {} to {}", name, definition.Fields[i].Name, DynamicMessage::FormatValue(original.GetValues()[i]), DynamicMessage::FormatValue(decoded.GetValues()[i]));
    }
    ByteBuffer again;
    decoded.Encode(again);
    if (!std::equal(bytes.begin(), bytes.end(), again.GetData().begin(), again.GetData().end()))
        return fmt::format("{}: re-encoding the decoded message changed its bytes", name);

    if (!bytes.empty())
    {
        DynamicMessage truncated(info);
        if (truncated.Decode(std::span<uint8 const>(bytes).first(bytes.size() - 1)) != MessageDecodeStatus::Truncated)
            return fmt::format("{}: a body one byte short was not reported as truncated", name);
    }
    std::vector<uint8> trailing = bytes;
    trailing.push_back(0x5A);
    DynamicMessage extra(info);
    if (extra.Decode(trailing) != MessageDecodeStatus::TrailingBytes)
        return fmt::format("{}: a body with an extra byte was not flagged", name);
    return std::string();
}
