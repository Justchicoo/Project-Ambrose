/*
 * Project Ambrose by Imjustchico
 * Streams a type dump through a JSON SAX handler that keeps only the fields the schema needs and refuses a known field of the wrong JSON type, then validates versions, duplicates, hashes, property ids, 32-bit option values, base chains, defaults and classes that hold themselves inline, measures the memory each default object takes, collapses pointer and SharedPointer aliases into their classes, matching unprefixed template names before inventing a class, classifies classes and property types, indexes enum options, and reports every problem with the class and property it belongs to.
 */

#include "TypeDumpLoader.h"
#include "PropertyDefaults.h"
#include "PropertyEnums.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace
{
    using Json = nlohmann::json;

    enum class Level : uint8
    {
        Root,
        Classes,
        Class,
        Bases,
        Properties,
        Property,
        Options,
        Skip
    };

    enum class JsonKind : uint8
    {
        Object,
        Array,
        String,
        Unsigned,
        Signed,
        Fraction,
        Boolean,
        Null,
        Binary
    };

    std::string_view Describe(JsonKind kind) noexcept
    {
        switch (kind)
        {
            case JsonKind::Object: return "an object";
            case JsonKind::Array: return "an array";
            case JsonKind::String: return "a string";
            case JsonKind::Unsigned: return "a non-negative integer";
            case JsonKind::Signed: return "a negative integer";
            case JsonKind::Fraction: return "a fractional number";
            case JsonKind::Boolean: return "a boolean";
            case JsonKind::Null: return "null";
            case JsonKind::Binary: return "binary data";
        }
        return "an unknown value";
    }

    std::optional<JsonKind> ExpectedKind(Level level, std::string_view key) noexcept
    {
        switch (level)
        {
            case Level::Root:
                if (key == "version")
                    return JsonKind::Unsigned;
                if (key == "classes")
                    return JsonKind::Object;
                return std::nullopt;
            case Level::Class:
                if (key == "name")
                    return JsonKind::String;
                if (key == "hash")
                    return JsonKind::Unsigned;
                if (key == "bases")
                    return JsonKind::Array;
                if (key == "properties")
                    return JsonKind::Object;
                return std::nullopt;
            case Level::Property:
                if (key == "type" || key == "container")
                    return JsonKind::String;
                if (key == "id" || key == "offset" || key == "flags" || key == "hash")
                    return JsonKind::Unsigned;
                if (key == "dynamic" || key == "singleton" || key == "pointer")
                    return JsonKind::Boolean;
                if (key == "enum_options")
                    return JsonKind::Object;
                return std::nullopt;
            default:
                return std::nullopt;
        }
    }

    class DumpReader final : public nlohmann::json_sax<Json>
    {
    public:
        DumpReader(TypeDumpLoader::RawDump& dump, std::vector<std::string>& errors) : _dump(dump), _errors(errors)
        {
        }

        bool null() override
        {
            return Accept(JsonKind::Null);
        }

        bool boolean(bool value) override
        {
            if (!Accept(JsonKind::Boolean))
                return false;
            if (Top() == Level::Property)
            {
                TypeDumpLoader::RawProperty& property = CurrentProperty();
                if (_key == "dynamic")
                    property.Dynamic = value;
                else if (_key == "singleton")
                    property.Singleton = value;
                else if (_key == "pointer")
                    property.Pointer = value;
            }
            return true;
        }

        bool number_integer(number_integer_t value) override
        {
            if (value >= 0)
                return number_unsigned(static_cast<number_unsigned_t>(value));
            if (Top() == Level::Options)
            {
                CurrentProperty().Options.emplace_back(_key, int64{ value });
                return true;
            }
            return Accept(JsonKind::Signed);
        }

        bool number_unsigned(number_unsigned_t value) override
        {
            if (Top() == Level::Options)
            {
                if (value > static_cast<number_unsigned_t>(std::numeric_limits<int64>::max()))
                    return Fail(fmt::format("{} has option {} with the value {}, which does not fit a 64-bit signed integer", Where(), _key, value));
                CurrentProperty().Options.emplace_back(_key, static_cast<int64>(value));
                return true;
            }
            if (!Accept(JsonKind::Unsigned))
                return false;
            switch (Top())
            {
                case Level::Root:
                    if (_key == "version")
                        _dump.Version = value > static_cast<number_unsigned_t>(std::numeric_limits<int64>::max()) ? -1 : static_cast<int64>(value);
                    break;
                case Level::Class:
                    if (_key == "hash")
                        CurrentClass().Hash = value;
                    break;
                case Level::Property:
                {
                    TypeDumpLoader::RawProperty& property = CurrentProperty();
                    if (_key == "id")
                        property.Id = value;
                    else if (_key == "offset")
                        property.Offset = value;
                    else if (_key == "flags")
                        property.Flags = value;
                    else if (_key == "hash")
                        property.Hash = value;
                    break;
                }
                default:
                    break;
            }
            return true;
        }

        bool number_float(number_float_t, string_t const&) override
        {
            return Accept(JsonKind::Fraction);
        }

        bool string(string_t& value) override
        {
            switch (Top())
            {
                case Level::Bases:
                    CurrentClass().Bases.push_back(std::move(value));
                    return true;
                case Level::Options:
                    CurrentProperty().Options.emplace_back(_key, std::move(value));
                    return true;
                default:
                    break;
            }
            if (!Accept(JsonKind::String))
                return false;
            if (Top() == Level::Class && _key == "name")
                CurrentClass().Name = std::move(value);
            else if (Top() == Level::Property && _key == "type")
                CurrentProperty().Type = std::move(value);
            else if (Top() == Level::Property && _key == "container")
                CurrentProperty().Container = std::move(value);
            return true;
        }

        bool binary(binary_t&) override
        {
            return Accept(JsonKind::Binary);
        }

        bool start_object(std::size_t) override
        {
            if (!_started)
            {
                _started = true;
                _stack.push_back(Level::Root);
                return true;
            }
            switch (Top())
            {
                case Level::Classes:
                    _dump.Classes.emplace_back().Key = _key;
                    _stack.push_back(Level::Class);
                    return true;
                case Level::Properties:
                    CurrentClass().Properties.emplace_back().Name = _key;
                    _stack.push_back(Level::Property);
                    return true;
                default:
                    break;
            }
            if (!Accept(JsonKind::Object))
                return false;
            Level next = Level::Skip;
            if (Top() == Level::Root && _key == "classes")
            {
                next = Level::Classes;
                _dump.HasClasses = true;
            }
            else if (Top() == Level::Class && _key == "properties")
                next = Level::Properties;
            else if (Top() == Level::Property && _key == "enum_options")
                next = Level::Options;
            _stack.push_back(next);
            return true;
        }

        bool key(string_t& value) override
        {
            _key = std::move(value);
            return true;
        }

        bool end_object() override
        {
            _stack.pop_back();
            return true;
        }

        bool start_array(std::size_t) override
        {
            if (!Accept(JsonKind::Array))
                return false;
            _stack.push_back(Top() == Level::Class && _key == "bases" ? Level::Bases : Level::Skip);
            return true;
        }

        bool end_array() override
        {
            _stack.pop_back();
            return true;
        }

        bool parse_error(std::size_t position, std::string const&, nlohmann::detail::exception const& error) override
        {
            return Fail(fmt::format("the type dump is not valid JSON at byte {}: {}", position, error.what()));
        }

    private:
        Level Top() const noexcept
        {
            return _stack.empty() ? Level::Root : _stack.back();
        }

        TypeDumpLoader::RawClass& CurrentClass()
        {
            return _dump.Classes.back();
        }

        TypeDumpLoader::RawProperty& CurrentProperty()
        {
            return _dump.Classes.back().Properties.back();
        }

        std::string ClassName() const
        {
            TypeDumpLoader::RawClass const& owner = _dump.Classes.back();
            return owner.Name.value_or("the class under key " + owner.Key);
        }

        std::string Where() const
        {
            switch (Top())
            {
                case Level::Classes:
                    return "the classes object";
                case Level::Class:
                case Level::Bases:
                case Level::Properties:
                    return ClassName();
                case Level::Property:
                case Level::Options:
                    return fmt::format("{} property {}", ClassName(), _dump.Classes.back().Properties.back().Name);
                default:
                    return "the type dump";
            }
        }

        bool Accept(JsonKind kind)
        {
            if (!_started)
                return Fail("the type dump is not a JSON object");
            switch (Top())
            {
                case Level::Skip:
                    return true;
                case Level::Classes:
                    return Fail(fmt::format("the classes object has {} under key {}, where a class object must be", Describe(kind), Ambrose::ForLog(_key)));
                case Level::Properties:
                    return Fail(fmt::format("{} has {} under property {}, where a property object must be", Where(), Describe(kind), Ambrose::ForLog(_key)));
                case Level::Bases:
                    return Fail(fmt::format("{} has {} among its bases, where only names may be", Where(), Describe(kind)));
                case Level::Options:
                    return Fail(fmt::format("{} has {} as option {}, where an integer or text must be", Where(), Describe(kind), Ambrose::ForLog(_key)));
                default:
                    break;
            }
            std::optional<JsonKind> const expected = ExpectedKind(Top(), _key);
            if (!expected || *expected == kind)
                return true;
            return Fail(fmt::format("{} has {} as {}, which must be {}", Where(), Describe(kind), _key, Describe(*expected)));
        }

        bool Fail(std::string message)
        {
            _errors.push_back(std::move(message));
            return false;
        }

        TypeDumpLoader::RawDump& _dump;
        std::vector<std::string>& _errors;
        std::vector<Level> _stack;
        std::string _key;
        bool _started = false;
    };

    struct ValueTypeName
    {
        std::string_view Name;
        ValueKind Kind;
    };

    constexpr ValueTypeName ValueTypes[] = {
        { "class Vector3D", ValueKind::Vector3D },
        { "class Quaternion", ValueKind::Quaternion },
        { "class Matrix3x3", ValueKind::Matrix3x3 },
        { "class Euler", ValueKind::Euler },
        { "class Color", ValueKind::Color },
        { "class Point<int>", ValueKind::PointInt },
        { "class Point<float>", ValueKind::PointFloat },
        { "class Size<int>", ValueKind::SizeInt },
        { "class Rect<int>", ValueKind::RectInt },
        { "class Rect<float>", ValueKind::RectFloat },
        { "class SerializedBuffer", ValueKind::SerializedBuffer },
        { "struct SimpleVert", ValueKind::SimpleVert },
        { "struct SimpleFace", ValueKind::SimpleFace }
    };

    constexpr ValueTypeName Primitives[] = {
        { "bool", ValueKind::Bool },
        { "char", ValueKind::Int8 },
        { "unsigned char", ValueKind::UInt8 },
        { "short", ValueKind::Int16 },
        { "unsigned short", ValueKind::UInt16 },
        { "int", ValueKind::Int32 },
        { "unsigned int", ValueKind::UInt32 },
        { "__int64", ValueKind::Int64 },
        { "unsigned __int64", ValueKind::UInt64 },
        { "gid", ValueKind::Gid },
        { "union gid", ValueKind::Gid },
        { "float", ValueKind::Float },
        { "double", ValueKind::Double },
        { "wchar_t", ValueKind::WideChar },
        { "std::string", ValueKind::String },
        { "std::wstring", ValueKind::WideString },
        { "s24", ValueKind::S24 },
        { "u24", ValueKind::U24 }
    };

    std::optional<ValueKind> FindValueType(std::string_view name) noexcept
    {
        for (ValueTypeName const& type : ValueTypes)
            if (type.Name == name)
                return type.Kind;
        return std::nullopt;
    }

    std::optional<std::pair<ValueKind, uint8>> FindPrimitive(std::string_view name) noexcept
    {
        for (ValueTypeName const& type : Primitives)
            if (type.Name == name)
                return std::pair{ type.Kind, uint8{ 0 } };
        bool const isUnsigned = name.starts_with("bui");
        if (!isUnsigned && !name.starts_with("bi"))
            return std::nullopt;
        std::optional<uint8> const width = Ambrose::StringTo<uint8>(name.substr(isUnsigned ? 3 : 2));
        if (!width || *width == 0 || *width > 32)
            return std::nullopt;
        return std::pair{ isUnsigned ? ValueKind::UnsignedBits : ValueKind::SignedBits, *width };
    }

    bool HasTypePrefix(std::string_view name) noexcept
    {
        return name.starts_with("class ") || name.starts_with("struct ") || name.starts_with("enum ") || name.starts_with("union ");
    }

    ClassKind Classify(std::string_view name, TypeDumpLoader::RawClass const& source) noexcept
    {
        if (name.starts_with("enum "))
            return ClassKind::Enum;
        if (!source.Properties.empty() || !source.Bases.empty() || name == "class PropertyClass")
            return ClassKind::PropertyClass;
        if (!name.starts_with("class ") && !name.starts_with("struct "))
            return ClassKind::Primitive;
        std::string_view const unqualified = name.substr(name.find(' ') + 1);
        if (unqualified.starts_with("std::"))
            return ClassKind::Container;
        if (FindValueType(name))
            return ClassKind::ValueType;
        return ClassKind::Opaque;
    }

    std::optional<uint32> Narrow(std::optional<uint64> value) noexcept
    {
        if (!value || *value > std::numeric_limits<uint32>::max())
            return std::nullopt;
        return static_cast<uint32>(*value);
    }

    std::size_t Capacity(std::string const& text) noexcept
    {
        return text.capacity() > sizeof(std::string) ? text.capacity() : 0;
    }

    template<typename Map>
    std::size_t MapBytes(Map const& map) noexcept
    {
        return map.bucket_count() * sizeof(void*) + map.size() * (sizeof(typename Map::value_type) + 2 * sizeof(void*));
    }

    void ReplaceAll(std::string& text, std::string_view from, std::string_view to)
    {
        for (std::size_t position = text.find(from); position != std::string::npos; position = text.find(from, position + to.size()))
            text.replace(position, from.size(), to);
    }
}

bool TypeDumpLoader::Parse(std::string_view text, RawDump& dump, std::vector<std::string>& errors)
{
    DumpReader reader(dump, errors);
    bool const parsed = Json::sax_parse(text.begin(), text.end(), &reader);
    return parsed && errors.empty();
}

std::string TypeDumpLoader::Canonicalize(std::string_view typeName)
{
    std::string_view name = typeName;
    while (true)
    {
        std::string_view const trimmed = Ambrose::Trim(name);
        if (trimmed.ends_with('*'))
        {
            name = trimmed.substr(0, trimmed.size() - 1);
            continue;
        }
        constexpr std::string_view SharedPointer = "class SharedPointer<";
        if (trimmed.starts_with(SharedPointer) && trimmed.ends_with('>'))
        {
            name = trimmed.substr(SharedPointer.size(), trimmed.size() - SharedPointer.size() - 1);
            continue;
        }
        return std::string(trimmed);
    }
}

std::string TypeDumpLoader::Normalize(std::string_view typeName)
{
    std::string text(Ambrose::Trim(typeName));
    ReplaceAll(text, "class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >", "std::string");
    ReplaceAll(text, "class std::basic_string<wchar_t,struct std::char_traits<wchar_t>,class std::allocator<wchar_t> >", "std::wstring");
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size();)
    {
        bool const atWord = index == 0 || !(std::isalnum(static_cast<unsigned char>(text[index - 1])) || text[index - 1] == '_' || text[index - 1] == ':');
        if (atWord && HasTypePrefix(std::string_view(text).substr(index)))
        {
            index = text.find(' ', index) + 1;
            continue;
        }
        char const character = text[index++];
        if (character == ' ' && (result.empty() || result.back() == ' ' || result.back() == '<' || result.back() == ','))
            continue;
        if ((character == '>' || character == ',') && !result.empty() && result.back() == ' ')
            result.pop_back();
        result.push_back(character);
    }
    return std::string(Ambrose::Trim(result));
}

TypeCatalogPtr TypeCatalogBuilder::Build(TypeDumpLoader::RawDump dump, std::string sourceName, std::string sha256, uint64 generation, std::vector<std::string>& errors)
{
    std::size_t const initialErrors = errors.size();
    if (dump.Version != TypeDumpLoader::SupportedVersion)
        errors.push_back(fmt::format("the type dump is format version {}, but only version {} is supported", dump.Version ? std::to_string(*dump.Version) : std::string("(missing)"), TypeDumpLoader::SupportedVersion));
    if (!dump.HasClasses)
        errors.push_back("the type dump has no classes object");
    else if (dump.Classes.empty())
        errors.push_back("the type dump lists no classes");

    std::unordered_set<std::string> seenNames;
    std::unordered_map<uint32, std::string> namesByHash;
    for (TypeDumpLoader::RawClass const& raw : dump.Classes)
    {
        if (!raw.Name)
        {
            errors.push_back(fmt::format("the class under key {} has no name", Ambrose::ForLog(raw.Key)));
            continue;
        }
        std::string const& name = *raw.Name;
        if (!seenNames.insert(name).second)
            errors.push_back(fmt::format("{} is listed twice", name));
        std::optional<uint32> const hash = Narrow(raw.Hash);
        uint32 const expected = StringHash::KiStringHash(name);
        if (!hash)
            errors.push_back(fmt::format("{} has no hash of at most 32 bits", name));
        else if (*hash != expected)
            errors.push_back(fmt::format("{} records hash {}, but its name hashes to {}", name, *hash, expected));
        else if (raw.Key != std::to_string(*hash))
            errors.push_back(fmt::format("{} is listed under key {} instead of its hash {}", name, Ambrose::ForLog(raw.Key), *hash));
        if (hash)
        {
            auto const [existing, inserted] = namesByHash.emplace(*hash, name);
            if (!inserted && existing->second != name)
                errors.push_back(fmt::format("{} and {} share the hash {}", existing->second, name, *hash));
        }

        std::set<std::string_view> propertyNames;
        for (TypeDumpLoader::RawProperty const& property : raw.Properties)
        {
            std::string const where = fmt::format("{} property {}", name, property.Name);
            std::vector<std::string_view> missing;
            for (auto const& [field, present] : { std::pair{ std::string_view("type"), property.Type.has_value() }, { std::string_view("container"), property.Container.has_value() },
                     { std::string_view("id"), property.Id.has_value() }, { std::string_view("offset"), property.Offset.has_value() }, { std::string_view("flags"), property.Flags.has_value() },
                     { std::string_view("hash"), property.Hash.has_value() }, { std::string_view("dynamic"), property.Dynamic.has_value() },
                     { std::string_view("singleton"), property.Singleton.has_value() }, { std::string_view("pointer"), property.Pointer.has_value() } })
                if (!present)
                    missing.push_back(field);
            if (!missing.empty())
            {
                errors.push_back(fmt::format("{} has no {}", where, fmt::join(missing, ", ")));
                continue;
            }
            if (!propertyNames.insert(property.Name).second)
                errors.push_back(fmt::format("{} is listed twice", where));
            if (*property.Container != "Static" && *property.Container != "List" && *property.Container != "Vector")
                errors.push_back(fmt::format("{} has container {}, which is not Static, List or Vector", where, Ambrose::ForLog(*property.Container)));
            if (!Narrow(property.Id) || !Narrow(property.Offset) || !Narrow(property.Flags))
                errors.push_back(fmt::format("{} has an id, offset or flags value above 32 bits", where));
            uint32 const expectedProperty = StringHash::PropertyHash(*property.Type, property.Name);
            if (Narrow(property.Hash) != expectedProperty)
                errors.push_back(fmt::format("{} records hash {}, but its type and name hash to {}", where, *property.Hash, expectedProperty));
            for (auto const& [optionName, value] : property.Options)
                if (optionName == "__BASECLASS" && !std::holds_alternative<std::string>(value))
                    errors.push_back(fmt::format("{} has a __BASECLASS option that is not text", where));
        }
    }
    if (errors.size() != initialErrors)
        return nullptr;

    auto catalog = std::shared_ptr<TypeCatalog>(new TypeCatalog());
    std::unordered_map<std::string, ClassInfo*> byCanonical;
    std::vector<std::pair<ClassInfo*, TypeDumpLoader::RawClass const*>> sources;
    std::unordered_map<std::string, std::vector<ClassInfo*>> byNormalized;
    auto const create = [&](std::string name, TypeDumpLoader::RawClass const& source) -> ClassInfo*
    {
        auto info = std::make_unique<ClassInfo>();
        info->Name = std::move(name);
        info->Hash = StringHash::KiStringHash(info->Name);
        info->Kind = Classify(info->Name, source);
        info->Owner = catalog.get();
        ClassInfo* const created = info.get();
        byCanonical.emplace(info->Name, created);
        sources.emplace_back(created, &source);
        catalog->_classes.push_back(std::move(info));
        return created;
    };
    for (TypeDumpLoader::RawClass const& raw : dump.Classes)
        if (TypeDumpLoader::Canonicalize(*raw.Name) == *raw.Name)
            byNormalized[TypeDumpLoader::Normalize(*raw.Name)].push_back(create(*raw.Name, raw));

    std::vector<std::pair<TypeDumpLoader::RawClass const*, ClassInfo const*>> aliases;
    for (TypeDumpLoader::RawClass const& raw : dump.Classes)
    {
        std::string canonical = TypeDumpLoader::Canonicalize(*raw.Name);
        if (canonical == *raw.Name)
            continue;
        ClassInfo* target = nullptr;
        if (auto const found = byCanonical.find(canonical); found != byCanonical.end())
            target = found->second;
        else if (auto const normalized = byNormalized.find(TypeDumpLoader::Normalize(canonical)); normalized != byNormalized.end() && normalized->second.size() == 1)
            target = normalized->second.front();
        else
            target = create(std::move(canonical), raw);
        aliases.emplace_back(&raw, target);
    }

    auto const findClass = [&byCanonical](std::string const& name) -> ClassInfo*
    {
        auto const found = byCanonical.find(name);
        return found == byCanonical.end() ? nullptr : found->second;
    };
    if (!findClass("class PropertyClass"))
        errors.push_back("the type dump has no class PropertyClass");

    for (auto const& [info, source] : sources)
    {
        for (std::string const& base : source->Bases)
        {
            ClassInfo* resolved = findClass(TypeDumpLoader::Canonicalize(base));
            if (!resolved && !HasTypePrefix(base))
            {
                resolved = findClass("class " + base);
                if (!resolved)
                    resolved = findClass("struct " + base);
            }
            if (!resolved)
                errors.push_back(fmt::format("{} names base {}, which the dump does not list", info->Name, base));
            else
                info->Bases.push_back(resolved);
        }

        std::vector<TypeDumpLoader::RawProperty const*> ordered;
        ordered.reserve(source->Properties.size());
        for (TypeDumpLoader::RawProperty const& property : source->Properties)
            ordered.push_back(&property);
        std::sort(ordered.begin(), ordered.end(), [](auto const* left, auto const* right) { return *left->Id < *right->Id; });
        for (uint32 index = 0; index < ordered.size(); ++index)
        {
            if (*ordered[index]->Id != index)
            {
                errors.push_back(fmt::format("{} property {} has id {}, but property ids must run from 0 to {}", info->Name, ordered[index]->Name, *ordered[index]->Id, ordered.size() - 1));
                break;
            }
        }

        info->Properties.reserve(ordered.size());
        for (TypeDumpLoader::RawProperty const* raw : ordered)
        {
            PropertyInfo& property = info->Properties.emplace_back();
            property.Name = raw->Name;
            property.Hash = static_cast<uint32>(*raw->Hash);
            property.Id = static_cast<uint32>(*raw->Id);
            property.Offset = static_cast<uint32>(*raw->Offset);
            property.Flags = static_cast<uint32>(*raw->Flags);
            property.Container = *raw->Container == "List" ? ContainerKind::List : *raw->Container == "Vector" ? ContainerKind::Vector : ContainerKind::Static;
            property.Dynamic = *raw->Dynamic;
            property.Singleton = *raw->Singleton;
            property.Pointer = *raw->Pointer;
            property.TypeName = *raw->Type;
            for (auto const& [optionName, value] : raw->Options)
            {
                if (optionName == "__DEFAULT")
                    property.Default = value;
                else if (optionName == "__BASECLASS")
                    property.OptionBaseClass = std::get<std::string>(value);
                else if (int64 const* const number = std::get_if<int64>(&value))
                {
                    std::optional<int64> const normalized = PropertyEnums::Normalize(*number);
                    if (!normalized)
                        errors.push_back(fmt::format("{} property {} has option {} with the value {}, which does not fit 32 bits", info->Name, property.Name, optionName, *number));
                    property.Options.push_back(EnumOption{ optionName, normalized.value_or(0) });
                }
                else
                    property.TextOptions.push_back(TextOption{ optionName, std::get<std::string>(value) });
            }
            property.OptionsByName.resize(property.Options.size());
            for (uint32 index = 0; index < property.Options.size(); ++index)
                property.OptionsByName[index] = index;
            property.OptionsByValue = property.OptionsByName;
            std::stable_sort(property.OptionsByName.begin(), property.OptionsByName.end(), [&property](uint32 left, uint32 right) { return property.Options[left].Name < property.Options[right].Name; });
            std::stable_sort(property.OptionsByValue.begin(), property.OptionsByValue.end(), [&property](uint32 left, uint32 right) { return property.Options[left].Value < property.Options[right].Value; });

            std::string const canonicalType = TypeDumpLoader::Canonicalize(property.TypeName);
            if (std::optional<std::pair<ValueKind, uint8>> const primitive = FindPrimitive(canonicalType))
            {
                property.Kind = primitive->first;
                property.BitWidth = primitive->second;
                property.Type = findClass(canonicalType);
                continue;
            }
            ClassInfo const* type = findClass(canonicalType);
            if (!type)
            {
                auto const normalized = byNormalized.find(TypeDumpLoader::Normalize(canonicalType));
                if (normalized != byNormalized.end() && normalized->second.size() == 1)
                    type = normalized->second.front();
            }
            if (!type)
            {
                errors.push_back(fmt::format("{} property {} has type {}, which the dump does not list", info->Name, property.Name, property.TypeName));
                continue;
            }
            property.Type = type;
            if (type->Kind == ClassKind::Enum)
                property.Kind = ValueKind::Enum;
            else if (type->Kind == ClassKind::PropertyClass)
                property.Kind = ValueKind::Object;
            else if (std::optional<ValueKind> const value = FindValueType(type->Name); value && type->Kind == ClassKind::ValueType)
                property.Kind = *value;
            else
                errors.push_back(fmt::format("{} property {} has type {}, {} {} that no value kind covers", info->Name, property.Name, property.TypeName,
                    type->Kind == ClassKind::Opaque || type->Kind == ClassKind::Enum ? "an" : "a", TypeKinds::GetName(type->Kind)));
        }
        for (uint32 index = 0; index < info->Properties.size(); ++index)
        {
            info->PropertyByHash.emplace(info->Properties[index].Hash, index);
            info->PropertyByName.emplace(info->Properties[index].Name, index);
        }
    }

    for (auto const& [info, source] : sources)
    {
        if (info->Bases.empty() || info->Bases.size() != source->Bases.size())
            continue;
        std::vector<ClassInfo const*> const inherited(info->Bases.begin() + 1, info->Bases.end());
        if (info->Bases.front()->Bases != inherited)
            errors.push_back(fmt::format("{} lists {} bases after {}, but {} itself lists {}", info->Name, inherited.size(), info->Bases.front()->Name, info->Bases.front()->Name, info->Bases.front()->Bases.size()));
    }
    if (errors.size() != initialErrors)
        return nullptr;

    for (std::unique_ptr<ClassInfo> const& info : catalog->_classes)
    {
        for (PropertyInfo& property : info->Properties)
        {
            std::string problem;
            if (std::optional<PropertyValue> resolved = PropertyDefaults::Resolve(property, problem))
                property.DefaultValue = std::move(*resolved);
            else
                errors.push_back(fmt::format("{} property {} {}", info->Name, property.Name, problem));
        }
    }

    enum class Visit : uint8 { New, Open, Done };
    std::unordered_map<ClassInfo const*, Visit> visits;
    std::vector<ClassInfo const*> finished;
    finished.reserve(catalog->_classes.size());
    for (std::unique_ptr<ClassInfo> const& root : catalog->_classes)
    {
        if (visits[root.get()] != Visit::New)
            continue;
        std::vector<std::pair<ClassInfo const*, std::size_t>> path{ { root.get(), 0 } };
        visits[root.get()] = Visit::Open;
        while (!path.empty())
        {
            auto& [current, next] = path.back();
            if (next == current->Properties.size())
            {
                visits[current] = Visit::Done;
                finished.push_back(current);
                path.pop_back();
                continue;
            }
            PropertyInfo const& property = current->Properties[next++];
            if (property.Kind != ValueKind::Object || property.Container != ContainerKind::Static || property.Pointer || !property.Type)
                continue;
            Visit& state = visits[property.Type];
            if (state == Visit::New)
            {
                state = Visit::Open;
                path.emplace_back(property.Type, 0);
            }
            else if (state == Visit::Open)
            {
                auto const start = std::find_if(path.begin(), path.end(), [&property](auto const& step) { return step.first == property.Type; });
                std::string chain;
                for (auto step = start; step != path.end(); ++step)
                    chain += fmt::format("{}{}.{}", chain.empty() ? "" : " -> ", step->first->Name, step->first->Properties[step->second - 1].Name);
                errors.push_back(fmt::format("{} holds itself inline through {}", property.Type->Name, chain));
            }
        }
    }
    if (errors.size() != initialErrors)
        return nullptr;

    for (ClassInfo const* done : finished)
    {
        ClassInfo& type = const_cast<ClassInfo&>(*done);
        type.DefaultBytes = sizeof(PropertyObject);
        for (PropertyInfo& property : type.Properties)
        {
            if (property.Container != ContainerKind::Static || property.Pointer)
                property.DefaultBytes = 0;
            else if (property.Kind == ValueKind::Object)
                property.DefaultBytes = property.Type ? property.Type->DefaultBytes : 0;
            else if (std::string const* const text = property.DefaultValue.GetIf<std::string>())
                property.DefaultBytes = text->size();
            else if (std::u16string const* const wide = property.DefaultValue.GetIf<std::u16string>())
                property.DefaultBytes = wide->size() * sizeof(char16_t);
            type.DefaultBytes += sizeof(PropertyValue) + property.DefaultBytes;
        }
    }

    std::size_t bytes = sizeof(TypeCatalog);
    for (std::unique_ptr<ClassInfo> const& info : catalog->_classes)
    {
        if (auto const [existing, inserted] = catalog->_byHash.emplace(info->Hash, info.get()); !inserted)
            errors.push_back(fmt::format("{} and {} share the hash {}", existing->second->Name, info->Name, info->Hash));
        catalog->_byName.emplace(info->Name, info.get());
        catalog->_classList.push_back(info.get());
        ++catalog->_kindCounts[static_cast<std::size_t>(info->Kind)];
        catalog->_propertyCount += info->Properties.size();
        bytes += sizeof(ClassInfo) + Capacity(info->Name) + info->Bases.capacity() * sizeof(ClassInfo const*) + MapBytes(info->PropertyByHash) + MapBytes(info->PropertyByName);
        for (PropertyInfo const& property : info->Properties)
        {
            bytes += sizeof(PropertyInfo) + Capacity(property.Name) + Capacity(property.TypeName) + Capacity(property.OptionBaseClass) + property.Options.capacity() * sizeof(EnumOption)
                + (property.OptionsByName.capacity() + property.OptionsByValue.capacity()) * sizeof(uint32) + property.TextOptions.capacity() * sizeof(TextOption);
            if (property.Default && std::holds_alternative<std::string>(*property.Default))
                bytes += Capacity(std::get<std::string>(*property.Default));
            if (std::string const* const text = property.DefaultValue.GetIf<std::string>())
                bytes += Capacity(*text);
            if (std::u16string const* const text = property.DefaultValue.GetIf<std::u16string>())
                bytes += text->capacity() * sizeof(char16_t);
            for (EnumOption const& option : property.Options)
                bytes += Capacity(option.Name);
            for (TextOption const& option : property.TextOptions)
                bytes += Capacity(option.Name) + Capacity(option.Text);
        }
    }
    for (auto const& [raw, target] : aliases)
    {
        ++catalog->_aliasCount;
        if (auto const [existing, inserted] = catalog->_byHash.emplace(static_cast<uint32>(*raw->Hash), target); !inserted && existing->second != target)
            errors.push_back(fmt::format("alias {} shares the hash {} with {}", *raw->Name, *raw->Hash, existing->second->Name));
        catalog->_byName.emplace(*raw->Name, target);
    }
    if (errors.size() != initialErrors)
        return nullptr;
    bytes += MapBytes(catalog->_byHash) + MapBytes(catalog->_byName) + catalog->_classList.capacity() * sizeof(ClassInfo const*);
    for (auto const& [name, info] : catalog->_byName)
        bytes += Capacity(name);
    catalog->_approximateBytes = bytes;
    catalog->_sourceName = std::move(sourceName);
    catalog->_sha256 = std::move(sha256);
    catalog->_generation = generation;
    return catalog;
}
