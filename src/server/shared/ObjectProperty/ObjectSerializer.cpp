/*
 * Project Ambrose by Imjustchico
 * Walks the class's properties in id order in the compact format, reading or writing a dirty-present bit where the property asks for one, and in the versionable format reads each object's size and each property's size and hash, fencing every value inside its declared bits, skipping and reporting unknown classes and properties and those the mask does not select, resynchronizing at a property's end when its value does not fit, and writes only the dirty-encoded properties the dirty test calls dirty; charges the depth and object count of every default inline object a written property takes; handles u16 or compact string lengths, u32 or compact counts, enums and flag integers as numbers or option names, bit fields at their width, and a class hash before every object, refusing counts the remaining bits cannot hold before allocating for them and charging every object, element, default, string and issue against the decode's memory budget; also opens and closes message fields by their envelope, class and null rules, and loads the decode limits from configuration into the snapshot each decode reads, through a per-thread copy refreshed when the snapshot changes, unless its options carry their own.
 */

#include "ObjectSerializer.h"
#include "BitReader.h"
#include "BitWriter.h"
#include "BlobEnvelope.h"
#include "ConfigMgr.h"
#include "PropertyEnums.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <optional>
#include <string>

namespace
{
    constexpr std::size_t NoIndex = std::numeric_limits<std::size_t>::max();
    constexpr SerializerFlag UnsupportedFlags = SerializerFlag::SerializeFlags | SerializerFlag::Compress;
    constexpr std::size_t ReserveCap = 4096;
    constexpr std::size_t ValueBytes = sizeof(PropertyValue);
    constexpr uint64 ShortLengthCeiling = uint64{ 1 } << 7;
    constexpr uint64 CompactLengthCeiling = uint64{ 1 } << 31;
    constexpr std::size_t ExcerptBytes = 64;

    struct LimitsStore
    {
        std::mutex Mutex;
        SerializerLimits Limits;
        std::atomic<uint64> Generation{ 1 };
    };

    LimitsStore& GetLimitsStore()
    {
        static LimitsStore store;
        return store;
    }

    std::string FieldName(ObjectField const& field)
    {
        return fmt::format("{}.{}", field.Message, field.Field);
    }

    std::string_view Excerpt(std::string_view text) noexcept
    {
        return text.substr(0, ExcerptBytes);
    }

    struct PathStep
    {
        PropertyInfo const* Property;
        std::size_t Index;
    };

    bool HasFlag(SerializerFlag flags, SerializerFlag flag) noexcept
    {
        return (flags & flag) != SerializerFlag::None;
    }

    bool IsTextNumber(PropertyInfo const& property, SerializerFlag flags) noexcept
    {
        if (!HasFlag(flags, SerializerFlag::StringEnums))
            return false;
        if (property.Kind == ValueKind::Enum)
            return true;
        return (property.Kind == ValueKind::Int32 || property.Kind == ValueKind::UInt32) && (property.HasFlag(PropertyFlag::Bits) || property.HasFlag(PropertyFlag::Enum));
    }

    std::size_t MinimumBits(PropertyInfo const& property, SerializerFlag flags) noexcept
    {
        std::size_t const lengthBits = HasFlag(flags, SerializerFlag::CompactLength) ? 8 : 16;
        if (IsTextNumber(property, flags))
            return lengthBits;
        switch (property.Kind)
        {
            case ValueKind::Bool: return 1;
            case ValueKind::Int8:
            case ValueKind::UInt8: return 8;
            case ValueKind::Int16:
            case ValueKind::UInt16:
            case ValueKind::WideChar: return 16;
            case ValueKind::String:
            case ValueKind::WideString: return lengthBits;
            case ValueKind::Int32:
            case ValueKind::UInt32:
            case ValueKind::Float:
            case ValueKind::Object:
            case ValueKind::Color:
            case ValueKind::Enum: return 32;
            case ValueKind::Int64:
            case ValueKind::UInt64:
            case ValueKind::Gid:
            case ValueKind::Double:
            case ValueKind::PointInt:
            case ValueKind::PointFloat:
            case ValueKind::SizeInt: return 64;
            case ValueKind::SignedBits:
            case ValueKind::UnsignedBits: return std::max<std::size_t>(property.BitWidth, 1);
            case ValueKind::S24:
            case ValueKind::U24: return 24;
            case ValueKind::Vector3D:
            case ValueKind::Euler: return 96;
            case ValueKind::Quaternion:
            case ValueKind::RectInt:
            case ValueKind::RectFloat: return 128;
            case ValueKind::Matrix3x3: return 288;
            case ValueKind::SerializedBuffer:
            case ValueKind::SimpleVert:
            case ValueKind::SimpleFace: return 0;
        }
        return 0;
    }

    template<typename T, std::size_t N>
    void ReadFields(BitReader& reader, std::array<T*, N> const& fields)
    {
        for (T* field : fields)
            *field = reader.Read<T>();
    }

    template<typename T, std::size_t N>
    void WriteFields(BitWriter& writer, std::array<T, N> const& fields)
    {
        for (T field : fields)
            writer.Write<T>(field);
    }

    class PathTracker
    {
    public:
        void Push(PropertyInfo const& property) { _steps.push_back({ &property, NoIndex }); }
        void Pop() noexcept { _steps.pop_back(); }
        void SetIndex(std::size_t index) noexcept { _steps.back().Index = index; }
        std::size_t GetDepth() const noexcept { return _steps.size(); }
        void Truncate(std::size_t depth) noexcept { _steps.resize(std::min(depth, _steps.size())); }

        std::string Format(ClassInfo const* root) const
        {
            std::string text = root ? root->Name : std::string("the object");
            for (PathStep const& step : _steps)
            {
                text += '.';
                text += step.Property->Name;
                if (step.Index != NoIndex)
                    fmt::format_to(std::back_inserter(text), "[{}]", step.Index);
            }
            return text;
        }

    private:
        std::vector<PathStep> _steps;
    };

    class Decoder
    {
    public:
        Decoder(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, SerializerOptions const& options, SerializerLimits const& limits, PropertyObject::BuildKey key)
            : _catalog(catalog), _reader(bytes), _options(options), _limits(limits), _key(key)
        {
        }

        DecodeResult Run(std::size_t size)
        {
            DecodeResult result;
            PropertyObjectPtr object;
            bool skipped = false;
            if (ReadObject(1, nullptr, object, skipped) && !object && !_options.AllowNullRoot)
                Fail(SerializerStatus::NullNotAllowed, "is null where an object is required");
            result.BytesRead = (_reader.GetBitPosition() + 7) / 8;
            if (_status == SerializerStatus::Ok && !_options.AllowTrailingBytes && result.BytesRead != size)
                Fail(SerializerStatus::TrailingBytes, fmt::format("ends after {} of {} bytes", result.BytesRead, size));
            result.Status = _status;
            if (_status == SerializerStatus::Ok)
            {
                result.Object = std::move(object);
                result.Issues = std::move(_issues);
            }
            else
            {
                result.Detail = fmt::format("{} {}", _failurePath, _failure);
            }
            return result;
        }

    private:
        bool Fail(SerializerStatus status, std::string_view what)
        {
            if (_status == SerializerStatus::Ok)
            {
                _status = status;
                _failurePath = _path.Format(_root);
                _failure = what;
            }
            return false;
        }

        bool Truncated()
        {
            return Fail(SerializerStatus::Truncated, "ends early");
        }

        bool Charge(std::size_t bytes)
        {
            if (bytes > _limits.MaxDecodedBytes - _charged)
                return Fail(SerializerStatus::BudgetExceeded, fmt::format("needs more than the {} bytes of memory a decode may use", _limits.MaxDecodedBytes));
            _charged += bytes;
            return true;
        }

        bool Report(DecodeIssueKind kind, uint32 hash, uint64 bits, std::string path, std::string detail)
        {
            if (!Charge(sizeof(DecodeIssue) + path.size() + detail.size()))
                return false;
            _issues.push_back(DecodeIssue{ kind, hash, bits, std::move(path), std::move(detail) });
            return true;
        }

        bool Recover(PropertyInfo const& property, uint64 bits, std::size_t pathDepth)
        {
            DecodeIssueKind kind = DecodeIssueKind::SizeMismatch;
            switch (_status)
            {
                case SerializerStatus::Truncated:
                case SerializerStatus::BadSize: kind = DecodeIssueKind::SizeMismatch; break;
                case SerializerStatus::UnsupportedType: kind = DecodeIssueKind::UnsupportedType; break;
                case SerializerStatus::UnknownEnumName: kind = DecodeIssueKind::UnknownEnumName; break;
                case SerializerStatus::WrongClass:
                case SerializerStatus::NotAPropertyClass:
                case SerializerStatus::NullNotAllowed: kind = DecodeIssueKind::InvalidObject; break;
                default: return false;
            }
            std::string path = std::move(_failurePath);
            std::string what = std::move(_failure);
            _status = SerializerStatus::Ok;
            _failurePath.clear();
            _failure.clear();
            _reader.ClearFailure();
            _path.Truncate(pathDepth);
            return Report(kind, property.Hash, bits, std::move(path), std::move(what));
        }

        bool ReadLength(bool text, uint64& length)
        {
            if (HasFlag(_options.Flags, SerializerFlag::CompactLength))
            {
                bool const wide = _reader.ReadBit();
                length = _reader.ReadBits(wide ? 31 : 7);
            }
            else
            {
                length = text ? uint64{ _reader.Read<uint16>() } : uint64{ _reader.Read<uint32>() };
            }
            if (_reader.Failed())
                return Truncated();
            return true;
        }

        bool ChargeDefault(PropertyInfo const& property, uint32 depth)
        {
            if (!Charge(property.DefaultBytes))
                return false;
            if (property.Kind != ValueKind::Object || property.Container != ContainerKind::Static || property.Pointer || !property.Type)
                return true;
            if (depth + property.Type->DefaultDepth > _depthLimit)
                return Fail(SerializerStatus::TooDeep, fmt::format("takes a default {} that nests objects deeper than {}", property.Type->Name, _depthLimit));
            if (property.Type->DefaultObjects > _limits.MaxObjects - std::min(_objects, _limits.MaxObjects))
                return Fail(SerializerStatus::TooManyObjects, fmt::format("holds more than {} objects", _limits.MaxObjects));
            _objects += property.Type->DefaultObjects;
            return true;
        }

        bool ReadObject(uint32 depth, ClassInfo const* expected, PropertyObjectPtr& out, bool& skipped)
        {
            uint32 const hash = _reader.Read<uint32>();
            if (_reader.Failed())
                return Truncated();
            if (hash == 0)
                return true;
            std::size_t objectEnd = 0;
            if (_versionable && !ReadObjectSize(objectEnd))
                return false;
            ClassInfo const* const type = _catalog->FindClass(hash);
            if (!type)
            {
                std::string what = fmt::format("names class hash {}, which the type dump does not list", hash);
                if (!_versionable || depth == 1)
                    return Fail(SerializerStatus::UnknownClass, what);
                skipped = true;
                std::size_t const bits = objectEnd - _reader.GetBitPosition();
                if (!Report(DecodeIssueKind::UnknownClass, hash, bits, _path.Format(_root), std::move(what)))
                    return false;
                _reader.SeekBit(objectEnd);
                return true;
            }
            if (type->Kind != ClassKind::PropertyClass)
                return Fail(SerializerStatus::NotAPropertyClass, fmt::format("names {}, which is not a property class", type->Name));
            if (expected && !type->IsA(*expected))
                return Fail(SerializerStatus::WrongClass, fmt::format("holds a {}, which is not a {}", type->Name, expected->Name));
            if (depth == 1 && !_options.RootClasses.empty()
                && std::none_of(_options.RootClasses.begin(), _options.RootClasses.end(), [type](ClassInfo const* allowed) { return allowed && type->IsA(*allowed); }))
                return Fail(SerializerStatus::WrongClass, fmt::format("names {}, which is not a class allowed here", type->Name));
            if (depth > _depthLimit)
                return Fail(SerializerStatus::TooDeep, fmt::format("nests objects deeper than {}", _depthLimit));
            if (++_objects > _limits.MaxObjects)
                return Fail(SerializerStatus::TooManyObjects, fmt::format("holds more than {} objects", _limits.MaxObjects));
            if (!_root)
                _root = type;
            if (!Charge(sizeof(PropertyObject) + type->Properties.size() * ValueBytes))
                return false;

            PropertyObjectPtr object = PropertyObject::CreateBlank(_key, _catalog, *type);
            std::vector<PropertyValue>& values = object->GetValues(_key);
            if (_versionable ? !ReadVersionableProperties(*type, *object, values, depth, objectEnd) : !ReadCompactProperties(*type, values, depth))
                return false;
            out = std::move(object);
            return true;
        }

        bool ReadObjectSize(std::size_t& objectEnd)
        {
            std::size_t const start = _reader.GetBitPosition();
            uint32 const size = _reader.Read<uint32>();
            if (_reader.Failed())
                return Truncated();
            if (size < 32)
                return Fail(SerializerStatus::BadSize, fmt::format("declares an object of {} bits, fewer than its own size field", size));
            if (size - 32 > _reader.GetRemainingBits())
                return Fail(SerializerStatus::BadSize, fmt::format("declares an object of {} bits, but only {} remain", size, _reader.GetRemainingBits() + 32));
            objectEnd = start + size;
            return true;
        }

        bool ReadCompactProperties(ClassInfo const& type, std::vector<PropertyValue>& values, uint32 depth)
        {
            for (std::size_t ordinal = 0; ordinal < type.Properties.size(); ++ordinal)
            {
                PropertyInfo const& property = type.Properties[ordinal];
                if (!ObjectSerializer::IsSelected(property, _options.Mask))
                {
                    if (!Charge(property.DefaultBytes))
                        return false;
                    values[ordinal] = PropertyObject::MakeDefault(_catalog, property);
                    continue;
                }
                _path.Push(property);
                if (property.HasFlag(PropertyFlag::DirtyEncode))
                {
                    bool const present = _reader.ReadBit();
                    if (_reader.Failed())
                        return Truncated();
                    if (!present)
                    {
                        if (!ChargeDefault(property, depth))
                            return false;
                        values[ordinal] = PropertyObject::MakeDefault(_catalog, property);
                        _path.Pop();
                        continue;
                    }
                }
                if (!ReadProperty(property, depth, values[ordinal]))
                    return false;
                _path.Pop();
            }
            return true;
        }

        bool ReadVersionableProperties(ClassInfo const& type, PropertyObject& object, std::vector<PropertyValue>& values, uint32 depth, std::size_t objectEnd)
        {
            std::size_t const outerLimit = _reader.GetLimit();
            _reader.SetLimit(objectEnd);
            std::vector<bool> seen(type.Properties.size(), false);
            while (_reader.GetBitPosition() < objectEnd)
            {
                std::size_t const start = _reader.GetBitPosition();
                uint32 const size = _reader.Read<uint32>();
                uint32 const hash = _reader.Read<uint32>();
                if (_reader.Failed())
                    return Fail(SerializerStatus::BadSize, fmt::format("has {} bits left, too few for a property header", objectEnd - start));
                std::size_t const header = _reader.GetBitPosition() - start;
                if (size < header)
                    return Fail(SerializerStatus::BadSize, fmt::format("declares a property of {} bits, fewer than its {}-bit header", size, header));
                if (size > objectEnd - start)
                    return Fail(SerializerStatus::BadSize, fmt::format("declares a property of {} bits, more than the {} left in its object", size, objectEnd - start));
                std::size_t const end = start + size;
                uint64 const valueBits = size - header;

                auto const found = type.PropertyByHash.find(hash);
                if (found == type.PropertyByHash.end())
                {
                    if (!Report(DecodeIssueKind::UnknownProperty, hash, valueBits, _path.Format(_root), fmt::format("holds property hash {}, which {} does not list", hash, type.Name)))
                        return false;
                    _reader.SeekBit(end);
                    continue;
                }
                uint32 const ordinal = found->second;
                PropertyInfo const& property = type.Properties[ordinal];
                object.MarkPresent(ordinal, _key);
                if (!ObjectSerializer::IsSelected(property, _options.Mask))
                {
                    std::string what = property.HasFlag(PropertyFlag::Deprecated) ? fmt::format("holds {}, which is deprecated", property.Name)
                                                                                   : fmt::format("holds {}, whose flags {:#x} the mask {:#x} does not select", property.Name, property.Flags, _options.Mask);
                    if (!Report(DecodeIssueKind::UnselectedProperty, hash, valueBits, _path.Format(_root), std::move(what)))
                        return false;
                    _reader.SeekBit(end);
                    continue;
                }
                std::size_t const pathDepth = _path.GetDepth();
                _path.Push(property);
                _reader.SetLimit(end);
                PropertyValue value;
                bool const read = ReadProperty(property, depth, value);
                std::size_t const consumed = _reader.GetBitPosition() - start - header;
                _reader.SetLimit(objectEnd);
                if (!read)
                {
                    if (!Recover(property, valueBits, pathDepth))
                        return false;
                }
                else
                {
                    if (consumed != valueBits
                        && !Report(DecodeIssueKind::SizeMismatch, property.Hash, valueBits, _path.Format(_root), fmt::format("fills {} of its {} bits", consumed, valueBits)))
                        return false;
                    values[ordinal] = std::move(value);
                    seen[ordinal] = true;
                    _path.Truncate(pathDepth);
                }
                _reader.SeekBit(end);
            }
            _reader.SetLimit(outerLimit);

            for (std::size_t ordinal = 0; ordinal < type.Properties.size(); ++ordinal)
            {
                if (seen[ordinal])
                    continue;
                PropertyInfo const& property = type.Properties[ordinal];
                if (ObjectSerializer::IsSelected(property, _options.Mask) ? !ChargeDefault(property, depth) : !Charge(property.DefaultBytes))
                    return false;
                values[ordinal] = PropertyObject::MakeDefault(_catalog, property);
            }
            return true;
        }

        bool ReadProperty(PropertyInfo const& property, uint32 depth, PropertyValue& out)
        {
            if (property.Container == ContainerKind::Static)
                return ReadElement(property, depth, out);
            std::size_t const minimum = MinimumBits(property, _options.Flags);
            if (minimum == 0)
                return Fail(SerializerStatus::UnsupportedType, fmt::format("has type {}, whose wire layout is not known", property.TypeName));
            uint64 count = 0;
            if (!ReadLength(false, count))
                return false;
            bool const fits = count <= _reader.GetRemainingBits() / minimum;
            if (_versionable && !fits)
                return Fail(SerializerStatus::Truncated, fmt::format("lists {} elements, more than the {} bytes left can hold", count, _reader.GetRemainingBits() / 8));
            if (count > _limits.MaxContainerCount)
                return Fail(SerializerStatus::ContainerTooLarge, fmt::format("lists {} elements, above the limit of {}", count, _limits.MaxContainerCount));
            if (!fits)
                return Fail(SerializerStatus::Truncated, fmt::format("lists {} elements, more than the {} bytes left can hold", count, _reader.GetRemainingBits() / 8));
            if (!Charge(static_cast<std::size_t>(count) * ValueBytes))
                return false;
            PropertyValue::List list;
            list.reserve(std::min<std::size_t>(static_cast<std::size_t>(count), ReserveCap));
            for (std::size_t index = 0; index < count; ++index)
            {
                _path.SetIndex(index);
                if (!ReadElement(property, depth, list.emplace_back()))
                    return false;
            }
            _path.SetIndex(NoIndex);
            out = std::move(list);
            return true;
        }

        bool ReadText(std::string& text)
        {
            uint64 length = 0;
            if (!ReadLength(true, length))
                return false;
            std::span<uint8 const> const bytes = _reader.ReadBytes(static_cast<std::size_t>(length));
            if (_reader.Failed())
                return Truncated();
            if (!Charge(bytes.size()))
                return false;
            text.assign(bytes.begin(), bytes.end());
            return true;
        }

        bool ReadTextNumber(PropertyInfo const& property, int64& value)
        {
            std::string text;
            if (!ReadText(text))
                return false;
            std::optional<int64> const parsed = PropertyEnums::Parse(property, text);
            if (!parsed || *parsed < 0 || *parsed > static_cast<int64>(std::numeric_limits<uint32>::max()))
                return Fail(SerializerStatus::UnknownEnumName, fmt::format("holds '{}', which names no option", Excerpt(text)));
            value = *parsed;
            return true;
        }

        bool ReadInlineDefault(PropertyInfo const& property, uint32 depth, PropertyValue& out)
        {
            if (!property.Type)
                return Fail(SerializerStatus::NullNotAllowed, "holds a null inline object");
            if (depth + property.Type->DefaultDepth > _depthLimit)
                return Fail(SerializerStatus::TooDeep, fmt::format("takes a default {} that nests objects deeper than {}", property.Type->Name, _depthLimit));
            if (property.Type->DefaultObjects > _limits.MaxObjects - std::min(_objects, _limits.MaxObjects))
                return Fail(SerializerStatus::TooManyObjects, fmt::format("holds more than {} objects", _limits.MaxObjects));
            _objects += property.Type->DefaultObjects;
            if (!Charge(property.Type->DefaultBytes))
                return false;
            PropertyObjectPtr object = PropertyObject::Create(_catalog, *property.Type);
            if (!object)
                return Fail(SerializerStatus::NullNotAllowed, "holds a null inline object");
            out = std::move(object);
            return true;
        }

        bool ReadElement(PropertyInfo const& property, uint32 depth, PropertyValue& out)
        {
            switch (property.Kind)
            {
                case ValueKind::Bool: out = _reader.ReadBit(); break;
                case ValueKind::Int8: out = _reader.Read<int8>(); break;
                case ValueKind::UInt8: out = _reader.Read<uint8>(); break;
                case ValueKind::Int16: out = _reader.Read<int16>(); break;
                case ValueKind::UInt16: out = _reader.Read<uint16>(); break;
                case ValueKind::Int32:
                case ValueKind::UInt32:
                case ValueKind::Enum:
                {
                    if (!IsTextNumber(property, _options.Flags))
                    {
                        if (property.Kind == ValueKind::Int32)
                            out = _reader.Read<int32>();
                        else if (property.Kind == ValueKind::UInt32)
                            out = _reader.Read<uint32>();
                        else
                            out = int64{ _reader.Read<uint32>() };
                        break;
                    }
                    int64 number = 0;
                    if (!ReadTextNumber(property, number))
                        return false;
                    if (property.Kind == ValueKind::Int32)
                        out = static_cast<int32>(static_cast<uint32>(number));
                    else if (property.Kind == ValueKind::UInt32)
                        out = static_cast<uint32>(number);
                    else
                        out = number;
                    break;
                }
                case ValueKind::Int64: out = _reader.Read<int64>(); break;
                case ValueKind::UInt64:
                case ValueKind::Gid: out = _reader.Read<uint64>(); break;
                case ValueKind::Float: out = _reader.Read<float>(); break;
                case ValueKind::Double: out = _reader.Read<double>(); break;
                case ValueKind::WideChar: out = static_cast<char16_t>(_reader.Read<uint16>()); break;
                case ValueKind::String:
                {
                    std::string text;
                    if (!ReadText(text))
                        return false;
                    out = std::move(text);
                    break;
                }
                case ValueKind::WideString:
                {
                    uint64 length = 0;
                    if (!ReadLength(true, length))
                        return false;
                    if (length > _reader.GetRemainingBits() / 16)
                        return Truncated();
                    std::span<uint8 const> const bytes = _reader.ReadBytes(static_cast<std::size_t>(length) * 2);
                    if (_reader.Failed())
                        return Truncated();
                    if (!Charge(bytes.size()))
                        return false;
                    std::u16string text(static_cast<std::size_t>(length), u'\0');
                    if constexpr (std::endian::native == std::endian::little)
                        std::memcpy(text.data(), bytes.data(), bytes.size());
                    else
                        for (std::size_t index = 0; index < text.size(); ++index)
                            text[index] = static_cast<char16_t>(bytes[index * 2] | (bytes[index * 2 + 1] << 8));
                    out = std::move(text);
                    break;
                }
                case ValueKind::SignedBits: out = static_cast<int32>(_reader.ReadSignedBits(property.BitWidth)); break;
                case ValueKind::UnsignedBits: out = static_cast<uint32>(_reader.ReadBits(property.BitWidth)); break;
                case ValueKind::S24: out = static_cast<int32>(_reader.ReadSignedBits(24)); break;
                case ValueKind::U24: out = static_cast<uint32>(_reader.ReadBits(24)); break;
                case ValueKind::Object:
                {
                    PropertyObjectPtr child;
                    bool skipped = false;
                    if (!ReadObject(depth + 1, property.Type, child, skipped))
                        return false;
                    if (!child && !property.Pointer)
                    {
                        if (skipped)
                            return ReadInlineDefault(property, depth, out);
                        return Fail(SerializerStatus::NullNotAllowed, "holds a null inline object");
                    }
                    out = std::move(child);
                    break;
                }
                case ValueKind::Vector3D:
                {
                    PropertyTypes::Vector3D value;
                    ReadFields<float, 3>(_reader, { &value.X, &value.Y, &value.Z });
                    out = value;
                    break;
                }
                case ValueKind::Quaternion:
                {
                    PropertyTypes::Quaternion value;
                    ReadFields<float, 4>(_reader, { &value.X, &value.Y, &value.Z, &value.W });
                    out = value;
                    break;
                }
                case ValueKind::Matrix3x3:
                {
                    PropertyTypes::Matrix3x3 value;
                    for (float& field : value.Values)
                        field = _reader.Read<float>();
                    out = value;
                    break;
                }
                case ValueKind::Euler:
                {
                    PropertyTypes::Euler value;
                    ReadFields<float, 3>(_reader, { &value.Pitch, &value.Yaw, &value.Roll });
                    out = value;
                    break;
                }
                case ValueKind::Color:
                {
                    PropertyTypes::Color value;
                    ReadFields<uint8, 4>(_reader, { &value.Red, &value.Green, &value.Blue, &value.Alpha });
                    out = value;
                    break;
                }
                case ValueKind::PointInt:
                {
                    PropertyTypes::PointInt value;
                    ReadFields<int32, 2>(_reader, { &value.X, &value.Y });
                    out = value;
                    break;
                }
                case ValueKind::PointFloat:
                {
                    PropertyTypes::PointFloat value;
                    ReadFields<float, 2>(_reader, { &value.X, &value.Y });
                    out = value;
                    break;
                }
                case ValueKind::SizeInt:
                {
                    PropertyTypes::SizeInt value;
                    ReadFields<int32, 2>(_reader, { &value.Width, &value.Height });
                    out = value;
                    break;
                }
                case ValueKind::RectInt:
                {
                    PropertyTypes::RectInt value;
                    ReadFields<int32, 4>(_reader, { &value.Left, &value.Top, &value.Right, &value.Bottom });
                    out = value;
                    break;
                }
                case ValueKind::RectFloat:
                {
                    PropertyTypes::RectFloat value;
                    ReadFields<float, 4>(_reader, { &value.Left, &value.Top, &value.Right, &value.Bottom });
                    out = value;
                    break;
                }
                case ValueKind::SerializedBuffer:
                case ValueKind::SimpleVert:
                case ValueKind::SimpleFace:
                    return Fail(SerializerStatus::UnsupportedType, fmt::format("has type {}, whose wire layout is not known", property.TypeName));
            }
            if (_reader.Failed())
                return Truncated();
            return true;
        }

        TypeCatalogPtr const& _catalog;
        BitReader _reader;
        SerializerOptions const& _options;
        SerializerLimits _limits;
        PropertyObject::BuildKey _key;
        bool _versionable = _options.Versionable;
        uint32 _depthLimit = std::min(_limits.MaxDepth, SerializerLimits::DepthCeiling);
        PathTracker _path;
        ClassInfo const* _root = nullptr;
        uint32 _objects = 0;
        std::size_t _charged = 0;
        SerializerStatus _status = SerializerStatus::Ok;
        std::string _failurePath;
        std::string _failure;
        std::vector<DecodeIssue> _issues;
    };

    class Encoder
    {
    public:
        Encoder(SerializerOptions const& options, SerializerLimits const& limits) : _options(options), _limits(limits)
        {
        }

        EncodeResult Run(PropertyObject const* object)
        {
            EncodeResult result;
            WriteObject(object, 1);
            result.Status = _status;
            result.Detail = std::move(_detail);
            if (_status == SerializerStatus::Ok)
                result.Bytes = _writer.TakeBytes();
            return result;
        }

    private:
        bool Fail(SerializerStatus status, std::string_view what)
        {
            if (_status == SerializerStatus::Ok)
            {
                _status = status;
                _detail = fmt::format("{} {}", _path.Format(_root), what);
            }
            return false;
        }

        bool Mismatch(PropertyInfo const& property)
        {
            return Fail(SerializerStatus::UnsupportedType, fmt::format("holds a value that is not of type {}", property.TypeName));
        }

        bool PatchSize(std::size_t start, std::size_t sizeField)
        {
            std::size_t const end = _writer.GetBitPosition();
            if (end - start > std::numeric_limits<uint32>::max())
                return Fail(SerializerStatus::ValueTooLong, fmt::format("takes {} bits, more than a u32 size can hold", end - start));
            _writer.SeekBit(sizeField);
            _writer.Write<uint32>(static_cast<uint32>(end - start));
            _writer.SeekBit(end);
            return true;
        }

        bool WriteObject(PropertyObject const* object, uint32 depth)
        {
            if (!object)
            {
                _writer.Write<uint32>(0);
                return true;
            }
            ClassInfo const& type = object->GetClass();
            if (depth > _depthLimit)
                return Fail(SerializerStatus::TooDeep, fmt::format("nests objects deeper than {}", _depthLimit));
            if (!_root)
                _root = &type;
            _writer.Write<uint32>(type.Hash);
            std::size_t const objectStart = _writer.GetBitPosition();
            if (_versionable)
                _writer.Write<uint32>(0);
            std::vector<std::size_t> const fallbackOrder = [&type]
            {
                std::vector<std::size_t> order;
                order.reserve(type.Properties.size());
                for (std::size_t ordinal = 0; ordinal < type.Properties.size(); ++ordinal)
                    order.push_back(ordinal);
                return order;
            }();
            std::vector<std::size_t> const& writeOrder = _versionable && object->HasPreservedOrder() && !object->GetPresentOrder().empty()
                ? object->GetPresentOrder()
                : fallbackOrder;
            for (std::size_t ordinal : writeOrder)
            {
                PropertyInfo const& property = type.Properties[ordinal];
                if (!ObjectSerializer::IsSelected(property, _options.Mask))
                    continue;
                bool const present = !property.HasFlag(PropertyFlag::DirtyEncode) || HasFlag(_options.Flags, SerializerFlag::ForceDirtyEncode) || !_options.IsDirty || _options.IsDirty(*object, property);
                _path.Push(property);
                if (_versionable)
                {
                    if (!present)
                    {
                        _path.Pop();
                        continue;
                    }
                    std::size_t const start = _writer.GetBitPosition();
                    _writer.Write<uint32>(0);
                    std::size_t const sizeField = _writer.GetBitPosition() - 32;
                    _writer.Write<uint32>(property.Hash);
                    if (!WriteProperty(property, *object->GetAt(ordinal), depth) || !PatchSize(start, sizeField))
                        return false;
                    _path.Pop();
                    continue;
                }
                if (property.HasFlag(PropertyFlag::DirtyEncode))
                {
                    _writer.WriteBit(present);
                    if (!present)
                    {
                        _path.Pop();
                        continue;
                    }
                }
                if (!WriteProperty(property, *object->GetAt(ordinal), depth))
                    return false;
                _path.Pop();
            }
            return !_versionable || PatchSize(objectStart, objectStart);
        }

        bool WriteLength(std::size_t length, bool text, std::string_view verb, std::string_view noun)
        {
            if (HasFlag(_options.Flags, SerializerFlag::CompactLength))
            {
                if (length >= CompactLengthCeiling)
                    return Fail(SerializerStatus::ValueTooLong, fmt::format("{} {} {}, more than a compact length can hold", verb, length, noun));
                bool const wide = length >= ShortLengthCeiling;
                _writer.WriteBit(wide);
                _writer.WriteBits(length, wide ? 31 : 7);
                return true;
            }
            if (text)
            {
                if (length > std::numeric_limits<uint16>::max())
                    return Fail(SerializerStatus::ValueTooLong, fmt::format("{} {} {}, more than a u16 length can hold", verb, length, noun));
                _writer.Write<uint16>(static_cast<uint16>(length));
                return true;
            }
            if (length > std::numeric_limits<uint32>::max())
                return Fail(SerializerStatus::ValueTooLong, fmt::format("{} {} {}, more than a u32 count can hold", verb, length, noun));
            _writer.Write<uint32>(static_cast<uint32>(length));
            return true;
        }

        bool WriteProperty(PropertyInfo const& property, PropertyValue const& value, uint32 depth)
        {
            if (property.Container == ContainerKind::Static)
                return WriteElement(property, value, depth);
            PropertyValue::List const* const list = value.GetList();
            if (!list)
                return Mismatch(property);
            if (!WriteLength(list->size(), false, "lists", "elements"))
                return false;
            for (std::size_t index = 0; index < list->size(); ++index)
            {
                _path.SetIndex(index);
                if (!WriteElement(property, (*list)[index], depth))
                    return false;
            }
            _path.SetIndex(NoIndex);
            return true;
        }

        bool WriteText(std::string_view text)
        {
            if (!WriteLength(text.size(), true, "holds", "bytes of text"))
                return false;
            _writer.WriteBytes(std::span<uint8 const>(reinterpret_cast<uint8 const*>(text.data()), text.size()));
            return true;
        }

        bool WriteTextNumber(PropertyInfo const& property, int64 number)
        {
            std::optional<std::string> const name = PropertyEnums::Format(property, number);
            return WriteText(name ? *name : std::to_string(number));
        }

        template<typename T>
        bool WriteScalar(PropertyInfo const& property, PropertyValue const& value)
        {
            T const* const scalar = value.GetIf<T>();
            if (!scalar)
                return Mismatch(property);
            _writer.Write<T>(*scalar);
            return true;
        }

        template<typename T>
        bool WriteNumber(PropertyInfo const& property, PropertyValue const& value)
        {
            T const* const number = value.GetIf<T>();
            if (!number)
                return Mismatch(property);
            if (IsTextNumber(property, _options.Flags))
                return WriteTextNumber(property, int64{ *number });
            _writer.Write<T>(*number);
            return true;
        }

        template<typename T>
        T const* Get(PropertyInfo const& property, PropertyValue const& value)
        {
            T const* const found = value.GetIf<T>();
            if (!found)
                Mismatch(property);
            return found;
        }

        bool WriteElement(PropertyInfo const& property, PropertyValue const& value, uint32 depth)
        {
            switch (property.Kind)
            {
                case ValueKind::Bool:
                {
                    bool const* const flag = Get<bool>(property, value);
                    if (!flag)
                        return false;
                    _writer.WriteBit(*flag);
                    return true;
                }
                case ValueKind::Int8: return WriteScalar<int8>(property, value);
                case ValueKind::UInt8: return WriteScalar<uint8>(property, value);
                case ValueKind::Int16: return WriteScalar<int16>(property, value);
                case ValueKind::UInt16: return WriteScalar<uint16>(property, value);
                case ValueKind::Int32: return WriteNumber<int32>(property, value);
                case ValueKind::UInt32: return WriteNumber<uint32>(property, value);
                case ValueKind::Int64: return WriteScalar<int64>(property, value);
                case ValueKind::UInt64:
                case ValueKind::Gid: return WriteScalar<uint64>(property, value);
                case ValueKind::Float: return WriteScalar<float>(property, value);
                case ValueKind::Double: return WriteScalar<double>(property, value);
                case ValueKind::WideChar:
                {
                    char16_t const* const character = Get<char16_t>(property, value);
                    if (!character)
                        return false;
                    _writer.Write<uint16>(static_cast<uint16>(*character));
                    return true;
                }
                case ValueKind::String:
                {
                    std::string const* const text = Get<std::string>(property, value);
                    return text && WriteText(*text);
                }
                case ValueKind::WideString:
                {
                    std::u16string const* const text = Get<std::u16string>(property, value);
                    if (!text || !WriteLength(text->size(), true, "holds", "UTF-16 units"))
                        return false;
                    if constexpr (std::endian::native == std::endian::little)
                        _writer.WriteBytes(std::span<uint8 const>(reinterpret_cast<uint8 const*>(text->data()), text->size() * sizeof(char16_t)));
                    else
                        for (char16_t unit : *text)
                            _writer.Write<uint16>(static_cast<uint16>(unit));
                    return true;
                }
                case ValueKind::SignedBits:
                case ValueKind::S24:
                {
                    int32 const* const number = Get<int32>(property, value);
                    if (!number)
                        return false;
                    _writer.WriteSignedBits(*number, property.Kind == ValueKind::S24 ? uint8{ 24 } : property.BitWidth);
                    return true;
                }
                case ValueKind::UnsignedBits:
                case ValueKind::U24:
                {
                    uint32 const* const number = Get<uint32>(property, value);
                    if (!number)
                        return false;
                    _writer.WriteBits(*number, property.Kind == ValueKind::U24 ? uint8{ 24 } : property.BitWidth);
                    return true;
                }
                case ValueKind::Enum:
                {
                    int64 const* const number = Get<int64>(property, value);
                    if (!number)
                        return false;
                    if (IsTextNumber(property, _options.Flags))
                        return WriteTextNumber(property, *number);
                    _writer.Write<uint32>(static_cast<uint32>(*number));
                    return true;
                }
                case ValueKind::Object:
                    if (!value.Holds<PropertyObjectPtr>())
                        return Mismatch(property);
                    return WriteObject(value.AsObject(), depth + 1);
                case ValueKind::Vector3D:
                {
                    PropertyTypes::Vector3D const* const vector = Get<PropertyTypes::Vector3D>(property, value);
                    if (!vector)
                        return false;
                    WriteFields<float, 3>(_writer, { vector->X, vector->Y, vector->Z });
                    return true;
                }
                case ValueKind::Quaternion:
                {
                    PropertyTypes::Quaternion const* const rotation = Get<PropertyTypes::Quaternion>(property, value);
                    if (!rotation)
                        return false;
                    WriteFields<float, 4>(_writer, { rotation->X, rotation->Y, rotation->Z, rotation->W });
                    return true;
                }
                case ValueKind::Matrix3x3:
                {
                    PropertyTypes::Matrix3x3 const* const matrix = Get<PropertyTypes::Matrix3x3>(property, value);
                    if (!matrix)
                        return false;
                    WriteFields<float, 9>(_writer, matrix->Values);
                    return true;
                }
                case ValueKind::Euler:
                {
                    PropertyTypes::Euler const* const angles = Get<PropertyTypes::Euler>(property, value);
                    if (!angles)
                        return false;
                    WriteFields<float, 3>(_writer, { angles->Pitch, angles->Yaw, angles->Roll });
                    return true;
                }
                case ValueKind::Color:
                {
                    PropertyTypes::Color const* const color = Get<PropertyTypes::Color>(property, value);
                    if (!color)
                        return false;
                    WriteFields<uint8, 4>(_writer, { color->Red, color->Green, color->Blue, color->Alpha });
                    return true;
                }
                case ValueKind::PointInt:
                {
                    PropertyTypes::PointInt const* const point = Get<PropertyTypes::PointInt>(property, value);
                    if (!point)
                        return false;
                    WriteFields<int32, 2>(_writer, { point->X, point->Y });
                    return true;
                }
                case ValueKind::PointFloat:
                {
                    PropertyTypes::PointFloat const* const point = Get<PropertyTypes::PointFloat>(property, value);
                    if (!point)
                        return false;
                    WriteFields<float, 2>(_writer, { point->X, point->Y });
                    return true;
                }
                case ValueKind::SizeInt:
                {
                    PropertyTypes::SizeInt const* const size = Get<PropertyTypes::SizeInt>(property, value);
                    if (!size)
                        return false;
                    WriteFields<int32, 2>(_writer, { size->Width, size->Height });
                    return true;
                }
                case ValueKind::RectInt:
                {
                    PropertyTypes::RectInt const* const rect = Get<PropertyTypes::RectInt>(property, value);
                    if (!rect)
                        return false;
                    WriteFields<int32, 4>(_writer, { rect->Left, rect->Top, rect->Right, rect->Bottom });
                    return true;
                }
                case ValueKind::RectFloat:
                {
                    PropertyTypes::RectFloat const* const rect = Get<PropertyTypes::RectFloat>(property, value);
                    if (!rect)
                        return false;
                    WriteFields<float, 4>(_writer, { rect->Left, rect->Top, rect->Right, rect->Bottom });
                    return true;
                }
                case ValueKind::SerializedBuffer:
                case ValueKind::SimpleVert:
                case ValueKind::SimpleFace:
                    return Fail(SerializerStatus::UnsupportedType, fmt::format("has type {}, whose wire layout is not known", property.TypeName));
            }
            return Mismatch(property);
        }

        SerializerOptions const& _options;
        SerializerLimits _limits;
        bool _versionable = _options.Versionable;
        uint32 _depthLimit = std::min(_limits.MaxDepth, SerializerLimits::DepthCeiling);
        BitWriter _writer;
        PathTracker _path;
        ClassInfo const* _root = nullptr;
        SerializerStatus _status = SerializerStatus::Ok;
        std::string _detail;
    };
}

DecodeResult ObjectSerializer::Decode(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, SerializerOptions const& options)
{
    DecodeResult result;
    if (!catalog)
    {
        result.Status = SerializerStatus::UnknownClass;
        result.Detail = "no type catalog is loaded";
        return result;
    }
    if (HasFlag(options.Flags, UnsupportedFlags))
    {
        result.Status = SerializerStatus::UnsupportedFlags;
        result.Detail = fmt::format("serializer flags {:#x} include a mode the object codec does not implement", static_cast<uint32>(options.Flags));
        return result;
    }
    try
    {
        return Decoder(catalog, bytes, options, options.Limits.value_or(SerializerLimits::Current()), PropertyObject::BuildKey()).Run(bytes.size());
    }
    catch (std::bad_alloc const&)
    {
        result.Status = SerializerStatus::OutOfMemory;
        result.Detail = "memory ran out while decoding";
        return result;
    }
}

EncodeResult ObjectSerializer::Encode(PropertyObject const* object, SerializerOptions const& options)
{
    if (HasFlag(options.Flags, UnsupportedFlags))
    {
        EncodeResult result;
        result.Status = SerializerStatus::UnsupportedFlags;
        result.Detail = fmt::format("serializer flags {:#x} include a mode the object codec does not implement", static_cast<uint32>(options.Flags));
        return result;
    }
    try
    {
        return Encoder(options, options.Limits.value_or(SerializerLimits::Current())).Run(object);
    }
    catch (std::bad_alloc const&)
    {
        EncodeResult result;
        result.Status = SerializerStatus::OutOfMemory;
        result.Detail = "memory ran out while encoding";
        return result;
    }
}

DecodeResult ObjectSerializer::DecodeField(TypeCatalogPtr const& catalog, ObjectField const& field, std::span<uint8 const> bytes, SerializerOptions options)
{
    DecodeResult result;
    auto const refuse = [&result, &field](SerializerStatus status, std::string detail)
    {
        result.Status = status;
        result.Detail = fmt::format("{} {}", FieldName(field), detail);
        return std::move(result);
    };
    if (!catalog)
        return refuse(SerializerStatus::UnknownClass, "cannot be read without a type catalog");
    options.RootClasses.clear();
    for (std::string_view const name : field.Classes)
    {
        ClassInfo const* const type = catalog->FindClass(name);
        if (!type || type->Kind != ClassKind::PropertyClass)
            return refuse(SerializerStatus::UnknownClass, fmt::format("allows {}, which the type dump does not list as a property class", name));
        options.RootClasses.push_back(type);
    }
    options.AllowNullRoot = field.AllowNull;
    if (!options.Limits)
        options.Limits = SerializerLimits::Current();

    BlobEnvelope::UnwrapResult unwrapped;
    std::span<uint8 const> payload = bytes;
    if (field.Enveloped)
    {
        try
        {
            unwrapped = BlobEnvelope::Unwrap(bytes, options.Limits->MaxInflatedSize);
        }
        catch (std::exception const& error)
        {
            return refuse(SerializerStatus::OutOfMemory, fmt::format("could not be inflated: {}", error.what()));
        }
        if (unwrapped.Code == BlobEnvelope::Status::OutOfMemory)
            return refuse(SerializerStatus::OutOfMemory, "ran out of memory while inflating");
        if (!unwrapped.Succeeded())
            return refuse(SerializerStatus::BadEnvelope, fmt::format("holds an envelope that cannot be opened: {}", BlobEnvelope::GetStatusName(unwrapped.Code)));
        payload = unwrapped.Data;
    }
    result = Decode(catalog, payload, options);
    if (!result.Ok())
        result.Detail = fmt::format("{}: {}", FieldName(field), result.Detail);
    else if (field.Enveloped)
        result.BytesRead = bytes.size();
    return result;
}

EncodeResult ObjectSerializer::EncodeField(ObjectField const& field, PropertyObject const* object, SerializerOptions options)
{
    EncodeResult result;
    auto const refuse = [&result, &field](SerializerStatus status, std::string detail)
    {
        result.Status = status;
        result.Detail = fmt::format("{} {}", FieldName(field), detail);
        return std::move(result);
    };
    if (!object && !field.AllowNull)
        return refuse(SerializerStatus::NullNotAllowed, "needs an object");
    if (object && std::none_of(field.Classes.begin(), field.Classes.end(), [object](std::string_view name) { return object->IsA(name); }))
        return refuse(SerializerStatus::WrongClass, fmt::format("cannot carry a {}", object->GetClass().Name));
    result = Encode(object, options);
    if (!result.Ok())
    {
        result.Detail = fmt::format("{}: {}", FieldName(field), result.Detail);
        return result;
    }
    if (field.Enveloped)
    {
        try
        {
            result.Bytes = BlobEnvelope::Wrap(result.Bytes, BlobEnvelope::Packing::Compress);
        }
        catch (std::length_error const&)
        {
            result.Bytes.clear();
            return refuse(SerializerStatus::ValueTooLong, "holds more bytes than an envelope can carry");
        }
        catch (std::exception const& error)
        {
            result.Bytes.clear();
            return refuse(SerializerStatus::OutOfMemory, fmt::format("could not be compressed: {}", error.what()));
        }
    }
    return result;
}

SerializerLimits SerializerLimits::Load(ConfigMgr const& config, std::vector<std::string>* problems)
{
    auto const bounded = [&config, problems](std::string const& option, uint64 fallback, uint64 minimum, uint64 maximum)
    {
        uint64 const configured = config.GetOption<uint64>(option, fallback, true);
        uint64 const value = std::clamp(configured, minimum, maximum);
        if (value != configured && problems)
            problems->push_back(fmt::format("{} = {} is outside {}-{}; using {}", option, configured, minimum, maximum, value));
        return value;
    };
    SerializerLimits const defaults;
    SerializerLimits limits;
    limits.MaxDepth = static_cast<uint32>(bounded("ObjectProperty.MaxDepth", defaults.MaxDepth, 1, DepthCeiling));
    limits.MaxObjects = static_cast<uint32>(bounded("ObjectProperty.MaxObjects", defaults.MaxObjects, 1, CountCeiling));
    limits.MaxContainerCount = static_cast<uint32>(bounded("ObjectProperty.MaxContainerCount", defaults.MaxContainerCount, 0, CountCeiling));
    limits.MaxDecodedBytes = static_cast<std::size_t>(bounded("ObjectProperty.MaxDecodedBytes", defaults.MaxDecodedBytes, DecodedBytesFloor, DecodedBytesCeiling));
    limits.MaxInflatedSize = static_cast<std::size_t>(bounded("ObjectProperty.MaxInflatedSize", defaults.MaxInflatedSize, InflatedSizeFloor, InflatedSizeCeiling));
    return limits;
}

SerializerLimits SerializerLimits::Current()
{
    thread_local uint64 seen = 0;
    thread_local SerializerLimits cached;
    LimitsStore& store = GetLimitsStore();
    if (store.Generation.load(std::memory_order_acquire) != seen)
    {
        std::lock_guard const lock(store.Mutex);
        cached = store.Limits;
        seen = store.Generation.load(std::memory_order_acquire);
    }
    return cached;
}

void SerializerLimits::Apply(SerializerLimits const& limits)
{
    LimitsStore& store = GetLimitsStore();
    std::lock_guard const lock(store.Mutex);
    store.Limits = limits;
    store.Generation.fetch_add(1, std::memory_order_acq_rel);
}

bool ObjectSerializer::IsSelected(PropertyInfo const& property, uint32 mask) noexcept
{
    return (property.Flags & mask) == mask && !property.HasFlag(PropertyFlag::Deprecated);
}

std::string_view ObjectSerializer::GetStatusName(SerializerStatus status) noexcept
{
    switch (status)
    {
        case SerializerStatus::Ok: return "ok";
        case SerializerStatus::Truncated: return "the data ends early";
        case SerializerStatus::TrailingBytes: return "bytes follow the object";
        case SerializerStatus::UnknownClass: return "an object names a class the type dump does not list";
        case SerializerStatus::NotAPropertyClass: return "an object names a type that is not a property class";
        case SerializerStatus::WrongClass: return "an object is not of its property's class";
        case SerializerStatus::NullNotAllowed: return "an inline object is null";
        case SerializerStatus::TooDeep: return "objects nest deeper than the limit";
        case SerializerStatus::TooManyObjects: return "the data holds more objects than the limit";
        case SerializerStatus::ContainerTooLarge: return "a list holds more elements than the limit";
        case SerializerStatus::BudgetExceeded: return "the decoded objects need more memory than the limit";
        case SerializerStatus::OutOfMemory: return "memory ran out";
        case SerializerStatus::BadEnvelope: return "the blob's envelope cannot be opened";
        case SerializerStatus::UnknownEnumName: return "an enum names no option";
        case SerializerStatus::ValueTooLong: return "a value is too long for its length field";
        case SerializerStatus::UnsupportedFlags: return "the serializer flags ask for a mode that is not implemented";
        case SerializerStatus::UnsupportedType: return "a value's wire layout is not known";
        case SerializerStatus::BadSize: return "a versionable object or property declares a size its data cannot have";
    }
    return "unknown";
}

std::string_view ObjectSerializer::GetIssueName(DecodeIssueKind kind) noexcept
{
    switch (kind)
    {
        case DecodeIssueKind::UnknownClass: return "unknown class";
        case DecodeIssueKind::UnknownProperty: return "unknown property";
        case DecodeIssueKind::SizeMismatch: return "size mismatch";
        case DecodeIssueKind::UnsupportedType: return "unsupported type";
        case DecodeIssueKind::UnknownEnumName: return "unknown enum name";
        case DecodeIssueKind::InvalidObject: return "invalid object";
        case DecodeIssueKind::UnselectedProperty: return "unselected property";
        case DecodeIssueKind::InvalidValue: return "invalid value";
    }
    return "unknown";
}
