/*
 * Project Ambrose by Imjustchico
 * Writes the fuzz catalog as a format v2 type dump and loads it; encodes a tree with plain, derived, null, pointer and inline children and a richly typed leaf, each with and without text enums, bare and inside stored and compressed envelopes, as the seeds; decodes an input by its mode under tight limits; and checks a decode is refused cleanly or re-encodes and decodes back equal.
 */

#include "ObjectFuzzCorpus.h"
#include "BlobEnvelope.h"
#include "StringHash.h"

#include <nlohmann/json.hpp>

#include <array>
#include <initializer_list>
#include <string_view>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;
    constexpr uint32 Dirty = Wire | 256;
    constexpr uint32 WireBits = Wire | (uint32{ 1 } << 20);

    Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags = Wire, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    Json LeafProperties()
    {
        Json leaf = Json::object();
        leaf["m_flag"] = Property("bool", "m_flag", 0);
        leaf["m_small"] = Property("bi5", "m_small", 1);
        leaf["m_tiny"] = Property("bui3", "m_tiny", 2);
        leaf["m_name"] = Property("std::string", "m_name", 3);
        leaf["m_title"] = Property("std::wstring", "m_title", 4, Dirty);
        Json mood = Property("enum FuzzMood", "m_mood", 5);
        mood["enum_options"] = Json{ { "kCalm", 0 }, { "kAngry", 4 } };
        leaf["m_mood"] = mood;
        leaf["m_wide"] = Property("s24", "m_wide", 6);
        leaf["m_values"] = Property("int", "m_values", 7, Wire, "Vector");
        leaf["m_flags"] = Property("bool", "m_flags", 8, Wire, "List");
        Json mask = Property("enum FuzzMask", "m_mask", 9, WireBits);
        mask["enum_options"] = Json{ { "A", 1 }, { "B", 2 }, { "AB", 3 } };
        leaf["m_mask"] = mask;
        return leaf;
    }

    bool Set(PropertyObject& object, std::string_view name, PropertyValue&& value)
    {
        return object.Set(name, std::move(value)) == PropertySetResult::Ok;
    }

    PropertyObjectPtr MakeLeaf(TypeCatalogPtr const& catalog, std::string const& className, int seed)
    {
        PropertyObjectPtr leaf = PropertyObject::Create(catalog, className);
        if (!leaf)
            return leaf;
        PropertyValue::List values;
        for (int index = 0; index < seed % 4; ++index)
            values.emplace_back(int32{ index * seed });
        PropertyValue::List flags;
        for (int index = 0; index < seed % 11; ++index)
            flags.emplace_back(index % 3 == 0);
        bool const complete = Set(*leaf, "m_flag", seed % 2 == 0) && Set(*leaf, "m_small", int32{ seed % 16 - 8 }) && Set(*leaf, "m_tiny", static_cast<uint32>(seed % 8))
            && Set(*leaf, "m_name", std::string(static_cast<std::size_t>(seed % 7), 'n')) && Set(*leaf, "m_title", std::u16string(static_cast<std::size_t>(seed % 5), u'w'))
            && Set(*leaf, "m_mood", int64{ seed % 2 == 0 ? 4 : 0 }) && Set(*leaf, "m_wide", int32{ -seed * 1000 }) && Set(*leaf, "m_values", std::move(values))
            && Set(*leaf, "m_flags", std::move(flags)) && Set(*leaf, "m_mask", int64{ seed % 4 });
        return complete ? std::move(leaf) : nullptr;
    }
}

TypeCatalogPtr ObjectFuzzCorpus::LoadCatalog(std::string& error)
{
    Json classes = Json::object();
    AddClass(classes, "class PropertyClass", Json::array(), Json::object());
    for (char const* name : { "enum FuzzMood", "enum FuzzMask", "class Vector3D", "class Quaternion", "class Color", "class Euler", "class Matrix3x3", "class Point<int>", "class Point<float>", "class Size<int>", "class Rect<int>", "class Rect<float>" })
        AddClass(classes, name, Json::array(), Json::object());
    AddClass(classes, "class FuzzLeaf", Json::array({ "PropertyClass" }), LeafProperties());

    Json rich = LeafProperties();
    rich["m_position"] = Property("class Vector3D", "m_position", 10);
    rich["m_rotation"] = Property("class Quaternion", "m_rotation", 11);
    rich["m_tint"] = Property("class Color", "m_tint", 12);
    rich["m_angles"] = Property("class Euler", "m_angles", 13);
    rich["m_matrix"] = Property("class Matrix3x3", "m_matrix", 14);
    rich["m_point"] = Property("class Point<int>", "m_point", 15);
    rich["m_pointF"] = Property("class Point<float>", "m_pointF", 16);
    rich["m_size"] = Property("class Size<int>", "m_size", 17);
    rich["m_rect"] = Property("class Rect<int>", "m_rect", 18);
    rich["m_rectF"] = Property("class Rect<float>", "m_rectF", 19);
    rich["m_id"] = Property("gid", "m_id", 20);
    rich["m_ratio"] = Property("double", "m_ratio", 21);
    rich["m_letter"] = Property("wchar_t", "m_letter", 22);
    rich["m_unsigned"] = Property("u24", "m_unsigned", 23);
    rich["m_shorts"] = Property("unsigned short", "m_shorts", 24, Wire, "List");
    AddClass(classes, "class FuzzRich", Json::array({ "FuzzLeaf", "PropertyClass" }), rich);

    Json tree = Json::object();
    tree["m_children"] = Property("class SharedPointer<class FuzzLeaf>", "m_children", 0, Wire, "List");
    tree["m_first"] = Property("class FuzzLeaf*", "m_first", 1);
    tree["m_inline"] = Property("class FuzzLeaf", "m_inline", 2);
    tree["m_branches"] = Property("class SharedPointer<class FuzzTree>", "m_branches", 3, Wire, "Vector");
    AddClass(classes, "class FuzzTree", Json::array({ "PropertyClass" }), tree);

    TypeRegistry registry;
    if (!registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "fuzz.json"))
    {
        error = registry.GetErrors().empty() ? std::string("the fuzz catalog did not load") : registry.GetErrors().front();
        return nullptr;
    }
    return registry.GetCatalog();
}

std::vector<ObjectFuzzCorpus::Seed> ObjectFuzzCorpus::MakeSeeds(TypeCatalogPtr const& catalog)
{
    std::vector<Seed> seeds;
    PropertyObjectPtr root = PropertyObject::Create(catalog, "class FuzzTree");
    PropertyObjectPtr rich = MakeLeaf(catalog, "class FuzzRich", 10);
    PropertyObjectPtr branch = PropertyObject::Create(catalog, "class FuzzTree");
    if (!root || !rich || !branch)
        return seeds;
    PropertyValue::List shorts;
    shorts.emplace_back(uint16{ 0x1234 });
    bool const richComplete = Set(*rich, "m_position", PropertyTypes::Vector3D{ 1.0f, 2.0f, 3.0f }) && Set(*rich, "m_tint", PropertyTypes::Color{ 1, 2, 3, 4 })
        && Set(*rich, "m_id", uint64{ 0x0123456789ABCDEFu }) && Set(*rich, "m_unsigned", uint32{ 0x123456 }) && Set(*rich, "m_shorts", std::move(shorts));
    PropertyValue::List children;
    children.emplace_back(MakeLeaf(catalog, "class FuzzLeaf", 3));
    children.emplace_back(PropertyObjectPtr());
    children.emplace_back(std::move(rich));
    PropertyValue::List branches;
    bool const branchComplete = Set(*branch, "m_first", MakeLeaf(catalog, "class FuzzRich", 7));
    branches.emplace_back(std::move(branch));
    bool const rootComplete = Set(*root, "m_children", std::move(children)) && Set(*root, "m_first", MakeLeaf(catalog, "class FuzzLeaf", 5)) && Set(*root, "m_branches", std::move(branches));
    PropertyObjectPtr const leaf = MakeLeaf(catalog, "class FuzzRich", 9);
    if (!richComplete || !branchComplete || !rootComplete || !leaf)
        return seeds;

    for (uint8 const mode : { uint8{ 0 }, TextEnums })
    {
        SerializerOptions const options = MakeOptions(mode);
        for (PropertyObject const* object : { root.get(), leaf.get() })
        {
            EncodeResult encoded = ObjectSerializer::EncodeCompact(object, options);
            if (!encoded.Ok())
                continue;
            seeds.push_back(Seed{ mode, encoded.Bytes });
            seeds.push_back(Seed{ static_cast<uint8>(mode | Enveloped), BlobEnvelope::Wrap(encoded.Bytes, BlobEnvelope::Packing::Store) });
            seeds.push_back(Seed{ static_cast<uint8>(mode | Enveloped), BlobEnvelope::Wrap(encoded.Bytes, BlobEnvelope::Packing::Compress) });
        }
    }
    return seeds;
}

ObjectField const& ObjectFuzzCorpus::GetEnvelopedField() noexcept
{
    static constexpr std::array<std::string_view, 2> Classes{ "class FuzzTree", "class FuzzLeaf" };
    static constexpr ObjectField Field{ "MSG_FUZZ", "Blob", Classes, true, true };
    return Field;
}

SerializerOptions ObjectFuzzCorpus::MakeOptions(uint8 mode)
{
    SerializerOptions options;
    options.Flags = (mode & TextEnums) != 0 ? SerializerFlag::StringEnums : SerializerFlag::None;
    options.AllowTrailingBytes = (mode & AllowTrailing) != 0;
    SerializerLimits& limits = options.Limits.emplace();
    limits.MaxDepth = 16;
    limits.MaxObjects = 256;
    limits.MaxContainerCount = 4096;
    limits.MaxDecodedBytes = std::size_t{ 1 } << 20;
    limits.MaxInflatedSize = std::size_t{ 1 } << 16;
    return options;
}

DecodeResult ObjectFuzzCorpus::Decode(TypeCatalogPtr const& catalog, uint8 mode, std::span<uint8 const> bytes)
{
    SerializerOptions const options = MakeOptions(mode);
    if ((mode & Enveloped) != 0)
        return ObjectSerializer::DecodeField(catalog, GetEnvelopedField(), bytes, options);
    return ObjectSerializer::DecodeCompact(catalog, bytes, options);
}

std::string ObjectFuzzCorpus::CheckDecoded(TypeCatalogPtr const& catalog, uint8 mode, std::span<uint8 const> bytes, DecodeResult const& decoded)
{
    if (decoded.BytesRead > bytes.size())
        return "the decoder claims to have read more bytes than it was given";
    if (!decoded.Ok())
    {
        if (decoded.Object)
            return "a refused decode returned an object";
        if (decoded.Detail.empty())
            return "a refused decode gave no detail";
        return {};
    }
    if (!decoded.Object)
        return {};
    SerializerOptions const options = MakeOptions(mode);
    bool const enveloped = (mode & Enveloped) != 0;
    EncodeResult const encoded = enveloped ? ObjectSerializer::EncodeField(GetEnvelopedField(), decoded.Object.get(), options) : ObjectSerializer::EncodeCompact(decoded.Object.get(), options);
    if (!encoded.Ok())
        return "a decoded object did not encode: " + encoded.Detail;
    DecodeResult const again = Decode(catalog, static_cast<uint8>(mode & ~AllowTrailing), encoded.Bytes);
    if (!again.Ok() || !again.Object)
        return "a re-encoded object did not decode: " + again.Detail;
    if (!(*again.Object == *decoded.Object))
        return "a re-encoded object decoded to a different object";
    return {};
}
