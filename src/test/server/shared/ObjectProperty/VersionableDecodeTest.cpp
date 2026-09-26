/*
 * Project Ambrose by Imjustchico
 * Tests the versionable ObjectProperty format on classes the test invents, against bytes the test assembles itself: a literal object byte for byte, unknown properties skipped and unknown nested classes skipped and reported with their paths, each with every property it holds by hash and size, compact lengths in their 7-bit and 31-bit forms for strings, wide strings and lists, enums and flag integers carried as option names, values that do not fit their declared size, properties the mask does not select and objects of the wrong class resynchronized at their property's end and reported, impossible object and property sizes refused where they are the object's own and reported where a property holds them, the decode limits including the objects and depth of default inline objects, clean dirty-encoded properties left out, compact lengths in the compact format, and whole trees round-tripping in every mode.
 */

#include "BitWriter.h"
#include "ObjectSerializer.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;
    constexpr uint32 SaveOnly = 1;
    constexpr uint32 Dirty = Wire | 256;
    constexpr uint32 WireBits = Wire | (uint32{ 1 } << 20);
    constexpr uint32 WireEnum = Wire | (uint32{ 1 } << 21);
    constexpr uint32 UnknownHash = 0x12345678u;

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

    uint32 Hash(std::string_view type, std::string_view name)
    {
        return StringHash::PropertyHash(type, name);
    }

    uint32 ClassHash(std::string_view name)
    {
        return StringHash::KiStringHash(name);
    }

    Json ItemProperties()
    {
        Json item = Json::object();
        item["m_id"] = Property("unsigned int", "m_id", 0);
        item["m_name"] = Property("std::string", "m_name", 1);
        item["m_shiny"] = Property("bool", "m_shiny", 2);
        Json flags = Property("unsigned int", "m_flags", 3, WireBits);
        flags["enum_options"] = Json{ { "FLAG_A", 1 }, { "FLAG_B", 2 }, { "FLAG_C", 4 } };
        item["m_flags"] = flags;
        Json kind = Property("enum TestKind", "m_kind", 4);
        kind["enum_options"] = Json{ { "KIND_NONE", 0 }, { "KIND_HAT", 7 } };
        item["m_kind"] = kind;
        item["m_title"] = Property("std::wstring", "m_title", 5);
        item["m_tags"] = Property("std::string", "m_tags", 6, Wire, "List");
        item["m_weight"] = Property("float", "m_weight", 7, Dirty);
        item["m_blob"] = Property("class SerializedBuffer", "m_blob", 8, SaveOnly);
        Json level = Property("int", "m_level", 9, WireEnum);
        level["enum_options"] = Json{ { "LEVEL_ONE", 1 } };
        item["m_level"] = level;
        return item;
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        for (char const* name : { "enum TestKind", "class Color", "class SerializedBuffer" })
            AddClass(classes, name, Json::array(), Json::object());
        AddClass(classes, "class TestItem", Json::array({ "PropertyClass" }), ItemProperties());

        Json socket = Json::object();
        socket["m_slot"] = Property("int", "m_slot", 0);
        socket["m_locked"] = Property("bool", "m_locked", 1);
        AddClass(classes, "class TestSocket", Json::array({ "PropertyClass" }), socket);

        Json hat = ItemProperties();
        hat["m_sockets"] = Property("class SharedPointer<class TestSocket>", "m_sockets", 10, Wire, "List");
        hat["m_tint"] = Property("class Color", "m_tint", 11);
        AddClass(classes, "class TestHat", Json::array({ "TestItem", "PropertyClass" }), hat);

        Json box = Json::object();
        box["m_items"] = Property("class SharedPointer<class TestItem>", "m_items", 0, Wire, "List");
        box["m_main"] = Property("class TestItem", "m_main", 1);
        box["m_extra"] = Property("class TestItem*", "m_extra", 2);
        box["m_note"] = Property("std::string", "m_note", 3);
        AddClass(classes, "class TestBox", Json::array({ "PropertyClass" }), box);

        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "versionable.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    struct Mark
    {
        std::size_t Start;
        std::size_t SizeField;
    };

    class Stream
    {
    public:
        Mark BeginObject(uint32 classHash)
        {
            _bits.Write<uint32>(classHash);
            std::size_t const start = _bits.GetBitPosition();
            _bits.Write<uint32>(0);
            return Mark{ start, start };
        }

        Mark BeginProperty(uint32 propertyHash)
        {
            std::size_t const start = _bits.GetBitPosition();
            _bits.Write<uint32>(0);
            std::size_t const sizeField = _bits.GetBitPosition() - 32;
            _bits.Write<uint32>(propertyHash);
            return Mark{ start, sizeField };
        }

        void FixedProperty(uint32 propertyHash, uint32 declaredSize)
        {
            _bits.Write<uint32>(declaredSize);
            _bits.Write<uint32>(propertyHash);
        }

        void End(Mark mark)
        {
            std::size_t const end = _bits.GetBitPosition();
            _bits.SeekBit(mark.SizeField);
            _bits.Write<uint32>(static_cast<uint32>(end - mark.Start));
            _bits.SeekBit(end);
        }

        void Null() { _bits.Write<uint32>(0); }
        void U32(uint32 value) { _bits.Write<uint32>(value); }
        void I32(int32 value) { _bits.Write<int32>(value); }
        void F32(float value) { _bits.Write<float>(value); }
        void Bit(bool value) { _bits.WriteBit(value); }
        void Byte(uint8 value) { _bits.Write<uint8>(value); }

        void Length(uint64 length, bool longForm = false)
        {
            bool const wide = longForm || length >= 128;
            _bits.WriteBit(wide);
            _bits.WriteBits(length, wide ? 31 : 7);
        }

        void Text(std::string_view text, bool longForm = false)
        {
            Length(text.size(), longForm);
            _bits.WriteBytes(std::span<uint8 const>(reinterpret_cast<uint8 const*>(text.data()), text.size()));
        }

        void Wide(std::u16string_view text)
        {
            Length(text.size());
            for (char16_t unit : text)
                _bits.Write<uint16>(static_cast<uint16>(unit));
        }

        std::vector<uint8> Take() { return _bits.TakeBytes(); }

    private:
        BitWriter _bits;
    };

    struct ItemFields
    {
        uint32 Id = 0;
        std::string Name;
        bool Shiny = false;
        std::string Flags;
        std::string Kind = "KIND_NONE";
        std::u16string Title;
        std::vector<std::string> Tags;
        float Weight = 0.0f;
        std::string Level = "0";
    };

    void WriteItemProperties(Stream& stream, ItemFields const& item)
    {
        auto const property = [&stream](std::string_view type, std::string_view name, auto&& write)
        {
            Mark const mark = stream.BeginProperty(Hash(type, name));
            write();
            stream.End(mark);
        };
        property("unsigned int", "m_id", [&] { stream.U32(item.Id); });
        property("std::string", "m_name", [&] { stream.Text(item.Name); });
        property("bool", "m_shiny", [&] { stream.Bit(item.Shiny); });
        property("unsigned int", "m_flags", [&] { stream.Text(item.Flags); });
        property("enum TestKind", "m_kind", [&] { stream.Text(item.Kind); });
        property("std::wstring", "m_title", [&] { stream.Wide(item.Title); });
        property("std::string", "m_tags", [&]
        {
            stream.Length(item.Tags.size());
            for (std::string const& tag : item.Tags)
                stream.Text(tag);
        });
        property("float", "m_weight", [&] { stream.F32(item.Weight); });
        property("int", "m_level", [&] { stream.Text(item.Level); });
    }

    void WriteSocket(Stream& stream, int32 slot, bool locked)
    {
        Mark const object = stream.BeginObject(ClassHash("class TestSocket"));
        Mark const slotProperty = stream.BeginProperty(Hash("int", "m_slot"));
        stream.I32(slot);
        stream.End(slotProperty);
        Mark const lockedProperty = stream.BeginProperty(Hash("bool", "m_locked"));
        stream.Bit(locked);
        stream.End(lockedProperty);
        stream.End(object);
    }

    void AppendU32(std::vector<uint8>& bytes, uint32 value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<uint8>(value >> shift));
    }

    SerializerOptions FileOptions()
    {
        SerializerOptions options;
        options.Versionable = true;
        options.Flags = SerializerFlag::CompactLength | SerializerFlag::StringEnums;
        return options;
    }

    class VersionableDecodeTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
        }

        DecodeResult DecodeFile(std::span<uint8 const> bytes, SerializerOptions const& options = FileOptions())
        {
            return ObjectSerializer::Decode(_catalog, bytes, options);
        }

        TypeCatalogPtr _catalog;
    };
}

TEST_F(VersionableDecodeTest, ALiteralObjectDecodesAndEncodesByteForByte)
{
    std::vector<uint8> literal;
    AppendU32(literal, ClassHash("class TestSocket"));
    AppendU32(literal, 193);
    AppendU32(literal, 96);
    AppendU32(literal, Hash("int", "m_slot"));
    AppendU32(literal, 3);
    AppendU32(literal, 65);
    AppendU32(literal, Hash("bool", "m_locked"));
    literal.push_back(0x01);
    ASSERT_EQ(literal.size(), 29u);

    DecodeResult const decoded = DecodeFile(literal);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(decoded.BytesRead, 29u);
    EXPECT_TRUE(decoded.Issues.empty());
    EXPECT_EQ(*decoded.Object->Get("m_slot")->GetIf<int32>(), 3);
    EXPECT_TRUE(*decoded.Object->Get("m_locked")->GetIf<bool>());

    EncodeResult const encoded = ObjectSerializer::Encode(decoded.Object.get(), FileOptions());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, literal);
}

TEST_F(VersionableDecodeTest, UnknownPropertiesAndNestedClassesAreSkippedAndReported)
{
    Stream stream;
    Mark const box = stream.BeginObject(ClassHash("class TestBox"));
    Mark const items = stream.BeginProperty(Hash("class SharedPointer<class TestItem>", "m_items"));
    stream.Length(3);
    {
        Mark const hat = stream.BeginObject(ClassHash("class TestHat"));
        Mark const id = stream.BeginProperty(Hash("unsigned int", "m_id"));
        stream.U32(1652259);
        stream.End(id);
        Mark const unknown = stream.BeginProperty(0xDEADBEEFu);
        stream.Text("ignored");
        stream.End(unknown);
        Mark const name = stream.BeginProperty(Hash("std::string", "m_name"));
        stream.Text("Crowns-S58-Hats");
        stream.End(name);
        Mark const sockets = stream.BeginProperty(Hash("class SharedPointer<class TestSocket>", "m_sockets"));
        stream.Length(2);
        WriteSocket(stream, 1, false);
        WriteSocket(stream, 2, true);
        stream.End(sockets);
        stream.End(hat);
    }
    {
        Mark const stranger = stream.BeginObject(UnknownHash);
        Mark const value = stream.BeginProperty(0x0BADF00Du);
        stream.U32(5);
        stream.End(value);
        stream.End(stranger);
    }
    stream.Null();
    stream.End(items);
    Mark const mainProperty = stream.BeginProperty(Hash("class TestItem", "m_main"));
    {
        Mark const stranger = stream.BeginObject(UnknownHash);
        stream.End(stranger);
    }
    stream.End(mainProperty);
    Mark const note = stream.BeginProperty(Hash("std::string", "m_note"));
    stream.Text("box");
    stream.End(note);
    stream.End(box);
    std::vector<uint8> const bytes = stream.Take();

    DecodeResult const decoded = DecodeFile(bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(decoded.BytesRead, bytes.size());
    PropertyValue::List const& list = *decoded.Object->Get("m_items")->GetList();
    ASSERT_EQ(list.size(), 3u);
    PropertyObject const* const hat = list[0].AsObject();
    ASSERT_NE(hat, nullptr);
    EXPECT_TRUE(hat->IsA("class TestHat"));
    EXPECT_EQ(*hat->Get("m_id")->GetIf<uint32>(), 1652259u);
    EXPECT_EQ(*hat->Get("m_name")->GetIf<std::string>(), "Crowns-S58-Hats");
    PropertyValue::List const& sockets = *hat->Get("m_sockets")->GetList();
    ASSERT_EQ(sockets.size(), 2u);
    EXPECT_EQ(*sockets[1].AsObject()->Get("m_slot")->GetIf<int32>(), 2);
    EXPECT_TRUE(*sockets[1].AsObject()->Get("m_locked")->GetIf<bool>());
    EXPECT_EQ(list[1].AsObject(), nullptr);
    EXPECT_EQ(list[2].AsObject(), nullptr);
    PropertyObject const* const mainItem = decoded.Object->Get("m_main")->AsObject();
    ASSERT_NE(mainItem, nullptr);
    EXPECT_EQ(mainItem->GetClass().Name, "class TestItem");
    EXPECT_EQ(*mainItem->Get("m_id")->GetIf<uint32>(), 0u);
    EXPECT_EQ(*decoded.Object->Get("m_note")->GetIf<std::string>(), "box");

    ASSERT_EQ(decoded.Issues.size(), 4u);
    EXPECT_EQ(decoded.Issues[0].Kind, DecodeIssueKind::UnknownProperty);
    EXPECT_EQ(decoded.Issues[0].Hash, 0xDEADBEEFu);
    EXPECT_EQ(decoded.Issues[0].Bits, 64u);
    EXPECT_EQ(decoded.Issues[0].Path, "class TestBox.m_items[0]");
    EXPECT_EQ(decoded.Issues[0].Detail, "holds property hash 3735928559, which class TestHat does not list");
    EXPECT_EQ(decoded.Issues[1].Kind, DecodeIssueKind::UnknownClass);
    EXPECT_EQ(decoded.Issues[1].Hash, UnknownHash);
    EXPECT_EQ(decoded.Issues[1].Bits, 96u);
    EXPECT_EQ(decoded.Issues[1].Path, "class TestBox.m_items[1]");
    EXPECT_EQ(decoded.Issues[1].Detail, "names class hash 305419896, which the type dump does not list");
    EXPECT_EQ(decoded.Issues[2].Kind, DecodeIssueKind::UnknownClassProperty) << "the unknown object's own property, which a schema probe names";
    EXPECT_EQ(decoded.Issues[2].Hash, 0x0BADF00Du);
    EXPECT_EQ(decoded.Issues[2].Owner, UnknownHash);
    EXPECT_EQ(decoded.Issues[2].Bits, 32u);
    EXPECT_EQ(decoded.Issues[2].Path, "class TestBox.m_items[1]");
    EXPECT_EQ(decoded.Issues[2].Detail, "holds property hash 195948557 in class hash 305419896, which the type dump does not list");
    EXPECT_EQ(decoded.Issues[3].Kind, DecodeIssueKind::UnknownClass);
    EXPECT_EQ(decoded.Issues[3].Bits, 0u);
    EXPECT_EQ(decoded.Issues[3].Path, "class TestBox.m_main");
    EXPECT_EQ(ObjectSerializer::GetIssueName(decoded.Issues[1].Kind), "unknown class");
    EXPECT_EQ(ObjectSerializer::GetIssueName(decoded.Issues[2].Kind), "property of an unknown class");
}

TEST_F(VersionableDecodeTest, LengthsOf128OrMoreTakeThe31BitForm)
{
    ItemFields item;
    item.Id = 7;
    item.Name = std::string(128, 'n');
    item.Shiny = true;
    item.Flags = "FLAG_A|FLAG_C";
    item.Kind = "KIND_HAT";
    item.Title = std::u16string(200, u'w');
    for (int index = 0; index < 130; ++index)
        item.Tags.push_back(std::string(static_cast<std::size_t>(index % 3 == 0 ? 300 : 127), static_cast<char>('a' + index % 26)));
    item.Weight = 2.5f;
    item.Level = "LEVEL_ONE";
    Stream stream;
    Mark const itemObject = stream.BeginObject(ClassHash("class TestItem"));
    WriteItemProperties(stream, item);
    stream.End(itemObject);
    std::vector<uint8> const bytes = stream.Take();

    std::vector<uint8> const nameStart{ 0x01, 0x01, 0x00, 0x00, 'n' };
    EXPECT_TRUE(std::search(bytes.begin(), bytes.end(), nameStart.begin(), nameStart.end()) != bytes.end());
    std::vector<uint8> const shortTag{ 0xFE, 'b', 'b' };
    EXPECT_TRUE(std::search(bytes.begin(), bytes.end(), shortTag.begin(), shortTag.end()) != bytes.end());
    std::vector<uint8> const longTag{ 0x59, 0x02, 0x00, 0x00, 'a', 'a' };
    EXPECT_TRUE(std::search(bytes.begin(), bytes.end(), longTag.begin(), longTag.end()) != bytes.end());

    DecodeResult const decoded = DecodeFile(bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_TRUE(decoded.Issues.empty());
    PropertyObject const& object = *decoded.Object;
    EXPECT_EQ(*object.Get("m_name")->GetIf<std::string>(), item.Name);
    EXPECT_TRUE(*object.Get("m_title")->GetIf<std::u16string>() == item.Title);
    PropertyValue::List const& tags = *object.Get("m_tags")->GetList();
    ASSERT_EQ(tags.size(), 130u);
    EXPECT_EQ(tags[0].GetIf<std::string>()->size(), 300u);
    EXPECT_EQ(tags[1].GetIf<std::string>()->size(), 127u);
    EXPECT_EQ(*object.Get("m_flags")->GetIf<uint32>(), 5u);
    EXPECT_EQ(*object.Get("m_kind")->GetIf<int64>(), 7);
    EXPECT_EQ(*object.Get("m_level")->GetIf<int32>(), 1);
    EXPECT_TRUE(*object.Get("m_shiny")->GetIf<bool>());
    EXPECT_EQ(*object.Get("m_weight")->GetIf<float>(), 2.5f);

    EncodeResult const encoded = ObjectSerializer::Encode(decoded.Object.get(), FileOptions());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, bytes);

    Stream longShort;
    Mark const shortObject = longShort.BeginObject(ClassHash("class TestItem"));
    Mark const name = longShort.BeginProperty(Hash("std::string", "m_name"));
    longShort.Text("short", true);
    longShort.End(name);
    longShort.End(shortObject);
    DecodeResult const accepted = DecodeFile(longShort.Take());
    ASSERT_TRUE(accepted.Ok()) << accepted.Detail;
    EXPECT_EQ(*accepted.Object->Get("m_name")->GetIf<std::string>(), "short");
}

TEST_F(VersionableDecodeTest, EnumsAndFlagIntegersTravelAsOptionNames)
{
    ItemFields item;
    item.Flags = "";
    item.Kind = "KIND_NONE";
    item.Level = "LEVEL_ONE";
    Stream stream;
    Mark const object = stream.BeginObject(ClassHash("class TestItem"));
    WriteItemProperties(stream, item);
    stream.End(object);
    std::vector<uint8> const bytes = stream.Take();
    DecodeResult const decoded = DecodeFile(bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(*decoded.Object->Get("m_flags")->GetIf<uint32>(), 0u);
    EncodeResult const encoded = ObjectSerializer::Encode(decoded.Object.get(), FileOptions());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, bytes);

    PropertyObjectPtr const numbered = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(numbered);
    ASSERT_EQ(numbered->Set("m_flags", uint32{ 9 }), PropertySetResult::Ok);
    ASSERT_EQ(numbered->Set("m_level", int32{ -5 }), PropertySetResult::Ok);
    EncodeResult const unnamed = ObjectSerializer::Encode(numbered.get(), FileOptions());
    ASSERT_TRUE(unnamed.Ok()) << unnamed.Detail;
    DecodeResult const back = DecodeFile(unnamed.Bytes);
    ASSERT_TRUE(back.Ok()) << back.Detail;
    EXPECT_EQ(*back.Object->Get("m_flags")->GetIf<uint32>(), 9u);
    EXPECT_EQ(*back.Object->Get("m_level")->GetIf<int32>(), -5);
    EXPECT_TRUE(*back.Object == *numbered);

    SerializerOptions numeric = FileOptions();
    numeric.Flags = SerializerFlag::CompactLength;
    EncodeResult const plain = ObjectSerializer::Encode(numbered.get(), numeric);
    ASSERT_TRUE(plain.Ok()) << plain.Detail;
    DecodeResult const plainBack = DecodeFile(plain.Bytes, numeric);
    ASSERT_TRUE(plainBack.Ok()) << plainBack.Detail;
    EXPECT_TRUE(*plainBack.Object == *numbered);
    EXPECT_LT(plain.Bytes.size(), unnamed.Bytes.size());
}

TEST_F(VersionableDecodeTest, ValuesThatDoNotFitTheirSizeResynchronizeAndAreReported)
{
    Stream stream;
    Mark const object = stream.BeginObject(ClassHash("class TestItem"));
    Mark const id = stream.BeginProperty(Hash("unsigned int", "m_id"));
    stream.U32(9);
    stream.Byte(0xAA);
    stream.End(id);
    Mark const name = stream.BeginProperty(Hash("std::string", "m_name"));
    stream.Length(10);
    stream.Byte('a');
    stream.Byte('b');
    stream.End(name);
    Mark const blob = stream.BeginProperty(Hash("class SerializedBuffer", "m_blob"));
    stream.U32(1);
    stream.End(blob);
    Mark const kind = stream.BeginProperty(Hash("enum TestKind", "m_kind"));
    stream.Text("KIND_MISSING");
    stream.End(kind);
    Mark const weight = stream.BeginProperty(Hash("float", "m_weight"));
    stream.F32(2.5f);
    stream.End(weight);
    Mark const shiny = stream.BeginProperty(Hash("bool", "m_shiny"));
    stream.Bit(true);
    stream.End(shiny);
    stream.End(object);
    std::vector<uint8> const bytes = stream.Take();

    SerializerOptions saved = FileOptions();
    saved.Mask = PropertyFlags::Bit(PropertyFlag::Save);
    DecodeResult const decoded = DecodeFile(bytes, saved);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(*decoded.Object->Get("m_id")->GetIf<uint32>(), 9u);
    EXPECT_EQ(*decoded.Object->Get("m_name")->GetIf<std::string>(), "");
    EXPECT_EQ(*decoded.Object->Get("m_kind")->GetIf<int64>(), 0);
    EXPECT_EQ(*decoded.Object->Get("m_weight")->GetIf<float>(), 2.5f);
    EXPECT_TRUE(*decoded.Object->Get("m_shiny")->GetIf<bool>());

    ASSERT_EQ(decoded.Issues.size(), 4u);
    EXPECT_EQ(decoded.Issues[0].Kind, DecodeIssueKind::SizeMismatch);
    EXPECT_EQ(decoded.Issues[0].Hash, Hash("unsigned int", "m_id"));
    EXPECT_EQ(decoded.Issues[0].Bits, 40u);
    EXPECT_EQ(decoded.Issues[0].Path, "class TestItem.m_id");
    EXPECT_EQ(decoded.Issues[0].Detail, "fills 32 of its 40 bits");
    EXPECT_EQ(decoded.Issues[1].Kind, DecodeIssueKind::SizeMismatch);
    EXPECT_EQ(decoded.Issues[1].Path, "class TestItem.m_name");
    EXPECT_EQ(decoded.Issues[1].Detail, "ends early");
    EXPECT_EQ(decoded.Issues[2].Kind, DecodeIssueKind::UnsupportedType);
    EXPECT_EQ(decoded.Issues[2].Path, "class TestItem.m_blob");
    EXPECT_EQ(decoded.Issues[3].Kind, DecodeIssueKind::UnknownEnumName);
    EXPECT_EQ(decoded.Issues[3].Detail, "holds 'KIND_MISSING', which names no option");

    DecodeResult const transmitted = DecodeFile(bytes);
    ASSERT_TRUE(transmitted.Ok()) << transmitted.Detail;
    ASSERT_EQ(transmitted.Issues.size(), 4u);
    EXPECT_EQ(transmitted.Issues[2].Kind, DecodeIssueKind::UnselectedProperty);
    EXPECT_EQ(transmitted.Issues[2].Path, "class TestItem");
    EXPECT_EQ(transmitted.Issues[2].Bits, 32u);
    EXPECT_EQ(transmitted.Issues[2].Detail, "holds m_blob, whose flags 0x1 the mask 0x18 does not select");
    EncodeResult const again = ObjectSerializer::Encode(transmitted.Object.get(), FileOptions());
    ASSERT_TRUE(again.Ok()) << again.Detail;
    DecodeResult const back = DecodeFile(again.Bytes);
    ASSERT_TRUE(back.Ok()) << back.Detail;
    EXPECT_TRUE(back.Issues.empty());
    EXPECT_TRUE(*back.Object == *transmitted.Object);
}

TEST_F(VersionableDecodeTest, ObjectsOfTheWrongClassOrNullInlineObjectsAreReportedInsideTheirProperty)
{
    Stream stream;
    Mark const box = stream.BeginObject(ClassHash("class TestBox"));
    Mark const mainProperty = stream.BeginProperty(Hash("class TestItem", "m_main"));
    stream.Null();
    stream.End(mainProperty);
    Mark const extra = stream.BeginProperty(Hash("class TestItem*", "m_extra"));
    WriteSocket(stream, 4, true);
    stream.End(extra);
    Mark const note = stream.BeginProperty(Hash("std::string", "m_note"));
    stream.Text("after");
    stream.End(note);
    stream.End(box);

    DecodeResult const decoded = DecodeFile(stream.Take());
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_NE(decoded.Object->Get("m_main")->AsObject(), nullptr);
    EXPECT_EQ(decoded.Object->Get("m_extra")->AsObject(), nullptr);
    EXPECT_EQ(*decoded.Object->Get("m_note")->GetIf<std::string>(), "after");
    ASSERT_EQ(decoded.Issues.size(), 2u);
    EXPECT_EQ(decoded.Issues[0].Kind, DecodeIssueKind::InvalidObject);
    EXPECT_EQ(decoded.Issues[0].Path, "class TestBox.m_main");
    EXPECT_EQ(decoded.Issues[0].Detail, "holds a null inline object");
    EXPECT_EQ(decoded.Issues[1].Kind, DecodeIssueKind::InvalidObject);
    EXPECT_EQ(decoded.Issues[1].Path, "class TestBox.m_extra");
    EXPECT_EQ(decoded.Issues[1].Detail, "holds a class TestSocket, which is not a class TestItem");
    EXPECT_EQ(ObjectSerializer::GetIssueName(decoded.Issues[1].Kind), "invalid object");
}

TEST_F(VersionableDecodeTest, CleanDirtyEncodedPropertiesAreLeftOutAndDecodeToTheirDefault)
{
    PropertyObjectPtr const item = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(item);
    ASSERT_EQ(item->Set("m_weight", 2.5f), PropertySetResult::Ok);
    SerializerOptions clean = FileOptions();
    clean.IsDirty = [](PropertyObject const&, PropertyInfo const& property) { return property.Name != "m_weight"; };
    std::vector<uint8> weightHash;
    for (int shift = 0; shift < 32; shift += 8)
        weightHash.push_back(static_cast<uint8>(Hash("float", "m_weight") >> shift));

    EncodeResult const omitted = ObjectSerializer::Encode(item.get(), clean);
    ASSERT_TRUE(omitted.Ok()) << omitted.Detail;
    EXPECT_TRUE(std::search(omitted.Bytes.begin(), omitted.Bytes.end(), weightHash.begin(), weightHash.end()) == omitted.Bytes.end());
    DecodeResult const decoded = DecodeFile(omitted.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(*decoded.Object->Get("m_weight")->GetIf<float>(), 0.0f);

    clean.Flags |= SerializerFlag::ForceDirtyEncode;
    EncodeResult const forced = ObjectSerializer::Encode(item.get(), clean);
    ASSERT_TRUE(forced.Ok()) << forced.Detail;
    EXPECT_TRUE(std::search(forced.Bytes.begin(), forced.Bytes.end(), weightHash.begin(), weightHash.end()) != forced.Bytes.end());
    EXPECT_EQ(forced.Bytes.size(), omitted.Bytes.size() + 12);
    DecodeResult const kept = DecodeFile(forced.Bytes);
    ASSERT_TRUE(kept.Ok()) << kept.Detail;
    EXPECT_EQ(*kept.Object->Get("m_weight")->GetIf<float>(), 2.5f);
}

TEST_F(VersionableDecodeTest, ImpossibleSizesAreRefusedWhereTheObjectOwnsThemAndReportedWhereAPropertyHoldsThem)
{
    std::vector<uint8> zero;
    AppendU32(zero, ClassHash("class TestSocket"));
    AppendU32(zero, 96);
    AppendU32(zero, 0);
    AppendU32(zero, Hash("int", "m_slot"));
    DecodeResult const zeroSize = DecodeFile(zero);
    EXPECT_EQ(zeroSize.Status, SerializerStatus::BadSize);
    EXPECT_EQ(zeroSize.Detail, "class TestSocket declares a property of 0 bits, fewer than its 64-bit header");
    EXPECT_FALSE(zeroSize.Object);

    std::vector<uint8> tiny;
    AppendU32(tiny, ClassHash("class TestSocket"));
    AppendU32(tiny, 16);
    EXPECT_EQ(DecodeFile(tiny).Detail, "the object declares an object of 16 bits, fewer than its own size field");

    std::vector<uint8> huge;
    AppendU32(huge, ClassHash("class TestSocket"));
    AppendU32(huge, 1000);
    AppendU32(huge, 0);
    DecodeResult const overrun = DecodeFile(huge);
    EXPECT_EQ(overrun.Status, SerializerStatus::BadSize);
    EXPECT_EQ(overrun.Detail, "the object declares an object of 1000 bits, but only 64 remain");

    std::vector<uint8> wide;
    AppendU32(wide, ClassHash("class TestSocket"));
    AppendU32(wide, 128);
    AppendU32(wide, 500);
    AppendU32(wide, Hash("int", "m_slot"));
    AppendU32(wide, 0);
    EXPECT_EQ(DecodeFile(wide).Detail, "class TestSocket declares a property of 500 bits, more than the 96 left in its object");

    std::vector<uint8> scraps;
    AppendU32(scraps, ClassHash("class TestSocket"));
    AppendU32(scraps, 72);
    scraps.insert(scraps.end(), { 0x60, 0x00, 0x00, 0x00, 0x00 });
    EXPECT_EQ(DecodeFile(scraps).Detail, "class TestSocket has 40 bits left, too few for a property header");

    std::vector<uint8> stranger;
    AppendU32(stranger, UnknownHash);
    AppendU32(stranger, 32);
    DecodeResult const unknownRoot = DecodeFile(stranger);
    EXPECT_EQ(unknownRoot.Status, SerializerStatus::UnknownClass);

    Stream nested;
    Mark const box = nested.BeginObject(ClassHash("class TestBox"));
    Mark const extra = nested.BeginProperty(Hash("class TestItem*", "m_extra"));
    Mark const item = nested.BeginObject(ClassHash("class TestItem"));
    nested.FixedProperty(Hash("unsigned int", "m_id"), 0);
    nested.U32(0);
    nested.End(item);
    nested.End(extra);
    Mark const note = nested.BeginProperty(Hash("std::string", "m_note"));
    nested.Text("after");
    nested.End(note);
    nested.End(box);
    std::vector<uint8> const nestedBytes = nested.Take();
    DecodeResult const reported = DecodeFile(nestedBytes);
    ASSERT_TRUE(reported.Ok()) << reported.Detail;
    EXPECT_EQ(reported.Object->Get("m_extra")->AsObject(), nullptr);
    EXPECT_EQ(*reported.Object->Get("m_note")->GetIf<std::string>(), "after");
    ASSERT_EQ(reported.Issues.size(), 1u);
    EXPECT_EQ(reported.Issues[0].Kind, DecodeIssueKind::SizeMismatch);
    EXPECT_EQ(reported.Issues[0].Path, "class TestBox.m_extra");
    EXPECT_EQ(reported.Issues[0].Detail, "declares a property of 0 bits, fewer than its 64-bit header");

    for (std::size_t cut = 1; cut < nestedBytes.size(); ++cut)
    {
        DecodeResult const truncated = DecodeFile(std::span<uint8 const>(nestedBytes.data(), cut));
        EXPECT_FALSE(truncated.Ok()) << cut;
        EXPECT_FALSE(truncated.Object) << cut;
        EXPECT_FALSE(truncated.Detail.empty()) << cut;
    }
}

TEST_F(VersionableDecodeTest, TheDecodeLimitsStillApply)
{
    Stream stream;
    Mark const box = stream.BeginObject(ClassHash("class TestBox"));
    Mark const items = stream.BeginProperty(Hash("class SharedPointer<class TestItem>", "m_items"));
    stream.Length(3);
    for (int index = 0; index < 3; ++index)
    {
        Mark const item = stream.BeginObject(ClassHash("class TestItem"));
        stream.End(item);
    }
    stream.End(items);
    stream.End(box);
    std::vector<uint8> const bytes = stream.Take();
    ASSERT_TRUE(DecodeFile(bytes).Ok());

    SerializerOptions shallow = FileOptions();
    shallow.Limits.emplace().MaxDepth = 1;
    DecodeResult const deep = DecodeFile(bytes, shallow);
    EXPECT_EQ(deep.Status, SerializerStatus::TooDeep);
    EXPECT_EQ(deep.Detail, "class TestBox.m_items[0] nests objects deeper than 1");

    SerializerOptions few = FileOptions();
    few.Limits.emplace().MaxObjects = 3;
    EXPECT_EQ(DecodeFile(bytes, few).Status, SerializerStatus::TooManyObjects);

    SerializerOptions poor = FileOptions();
    poor.Limits.emplace().MaxDecodedBytes = 64;
    EXPECT_EQ(DecodeFile(bytes, poor).Status, SerializerStatus::BudgetExceeded);

    SerializerOptions narrow = FileOptions();
    narrow.Limits.emplace().MaxContainerCount = 2;
    EXPECT_EQ(DecodeFile(bytes, narrow).Status, SerializerStatus::ContainerTooLarge);

    Stream bomb;
    Mark const bombBox = bomb.BeginObject(ClassHash("class TestBox"));
    Mark const bombItems = bomb.BeginProperty(Hash("class SharedPointer<class TestItem>", "m_items"));
    bomb.Length(uint64{ 1 } << 24, true);
    bomb.Null();
    bomb.End(bombItems);
    bomb.End(bombBox);
    DecodeResult const refused = DecodeFile(bomb.Take());
    ASSERT_TRUE(refused.Ok()) << refused.Detail;
    ASSERT_EQ(refused.Issues.size(), 1u);
    EXPECT_EQ(refused.Issues[0].Kind, DecodeIssueKind::SizeMismatch);
    EXPECT_EQ(refused.Issues[0].Detail, "lists 16777216 elements, more than the 4 bytes left can hold");
    EXPECT_TRUE(refused.Object->Get("m_items")->GetList()->empty());

    Stream bare;
    Mark const bareBox = bare.BeginObject(ClassHash("class TestBox"));
    Mark const bareNote = bare.BeginProperty(Hash("std::string", "m_note"));
    bare.Text("only a note");
    bare.End(bareNote);
    bare.End(bareBox);
    std::vector<uint8> const bareBytes = bare.Take();
    ASSERT_TRUE(DecodeFile(bareBytes, shallow).Status == SerializerStatus::TooDeep);
    EXPECT_EQ(DecodeFile(bareBytes, shallow).Detail, "class TestBox takes a default class TestItem that nests objects deeper than 1");
    SerializerOptions single = FileOptions();
    single.Limits.emplace().MaxObjects = 1;
    EXPECT_EQ(DecodeFile(bareBytes, single).Status, SerializerStatus::TooManyObjects);
    single.Limits->MaxObjects = 2;
    EXPECT_TRUE(DecodeFile(bareBytes, single).Ok());
}

TEST_F(VersionableDecodeTest, TheCompactFormatReadsAndWritesCompactLengths)
{
    PropertyObjectPtr const item = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(item);
    ASSERT_EQ(item->Set("m_id", uint32{ 0x01020304 }), PropertySetResult::Ok);
    ASSERT_EQ(item->Set("m_name", "Hello"), PropertySetResult::Ok);
    PropertyValue::List tags;
    tags.emplace_back(std::string(200, 't'));
    ASSERT_EQ(item->Set("m_tags", std::move(tags)), PropertySetResult::Ok);
    SerializerOptions compact;
    compact.Flags = SerializerFlag::CompactLength;
    EncodeResult const encoded = ObjectSerializer::Encode(item.get(), compact);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    std::vector<uint8> prefix;
    AppendU32(prefix, ClassHash("class TestItem"));
    AppendU32(prefix, 0x01020304);
    prefix.insert(prefix.end(), { 0x0A, 'H', 'e', 'l', 'l', 'o' });
    ASSERT_GE(encoded.Bytes.size(), prefix.size());
    EXPECT_TRUE(std::equal(prefix.begin(), prefix.end(), encoded.Bytes.begin()));
    DecodeResult const decoded = ObjectSerializer::Decode(_catalog, encoded.Bytes, compact);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_TRUE(*decoded.Object == *item);
    EXPECT_FALSE(ObjectSerializer::Decode(_catalog, encoded.Bytes).Ok());
}

TEST_F(VersionableDecodeTest, WholeTreesRoundTripInEveryMode)
{
    PropertyObjectPtr const box = PropertyObject::Create(_catalog, "class TestBox");
    PropertyObjectPtr const hat = PropertyObject::Create(_catalog, "class TestHat");
    PropertyObjectPtr const socket = PropertyObject::Create(_catalog, "class TestSocket");
    ASSERT_TRUE(box && hat && socket);
    ASSERT_EQ(socket->Set("m_slot", int32{ -4 }), PropertySetResult::Ok);
    ASSERT_EQ(socket->Set("m_locked", true), PropertySetResult::Ok);
    PropertyValue::List sockets;
    sockets.emplace_back(socket->Clone());
    sockets.emplace_back(PropertyObjectPtr());
    ASSERT_EQ(hat->Set("m_sockets", std::move(sockets)), PropertySetResult::Ok);
    ASSERT_EQ(hat->Set("m_tint", PropertyTypes::Color{ 1, 2, 3, 4 }), PropertySetResult::Ok);
    ASSERT_EQ(hat->Set("m_title", u"Hat"), PropertySetResult::Ok);
    ASSERT_EQ(hat->Set("m_flags", uint32{ 6 }), PropertySetResult::Ok);
    ASSERT_EQ(hat->Set("m_kind", int64{ 7 }), PropertySetResult::Ok);
    PropertyValue::List items;
    items.emplace_back(hat->Clone());
    items.emplace_back(PropertyObjectPtr());
    ASSERT_EQ(box->Set("m_items", std::move(items)), PropertySetResult::Ok);
    ASSERT_EQ(box->Set("m_extra", hat->Clone()), PropertySetResult::Ok);
    ASSERT_EQ(box->Set("m_note", std::string(1000, 'x')), PropertySetResult::Ok);

    for (SerializerFlag const flags : { SerializerFlag::None, SerializerFlag::CompactLength, SerializerFlag::StringEnums, SerializerFlag::CompactLength | SerializerFlag::StringEnums })
    {
        for (bool const versionable : { false, true })
        {
            SerializerOptions options;
            options.Flags = flags;
            options.Versionable = versionable;
            EncodeResult const encoded = ObjectSerializer::Encode(box.get(), options);
            ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
            DecodeResult const decoded = ObjectSerializer::Decode(_catalog, encoded.Bytes, options);
            ASSERT_TRUE(decoded.Ok()) << static_cast<uint32>(flags) << ' ' << versionable << ": " << decoded.Detail;
            EXPECT_TRUE(decoded.Issues.empty());
            EXPECT_TRUE(*decoded.Object == *box) << static_cast<uint32>(flags) << ' ' << versionable;
        }
    }
}
