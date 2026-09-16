/*
 * Project Ambrose by Imjustchico
 * Tests the compact ObjectProperty codec on classes the test invents: golden bytes for mask filtering, skipped deprecated properties, packed bools and bit fields, the dirty-present bit and every other value layout the codec writes, a nested list of derived and null children round-tripping exactly, alias hashes, text enums and flag names, root class rules, and hostile or malformed data refused with the status and property path it failed at, including the memory budget and the depth ceiling.
 */

#include "ObjectSerializer.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;
    constexpr uint32 WirePublic = Wire | 4;
    constexpr uint32 SaveOnly = 1;
    constexpr uint32 Deprecated = Wire | 64;
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

    Json CardProperties()
    {
        Json properties = Json::object();
        properties["m_id"] = Property("unsigned int", "m_id", 0);
        properties["m_secret"] = Property("int", "m_secret", 1, SaveOnly);
        properties["m_old"] = Property("float", "m_old", 2, Deprecated);
        properties["m_name"] = Property("std::string", "m_name", 3, WirePublic);
        properties["m_shiny"] = Property("bool", "m_shiny", 4);
        properties["m_tier"] = Property("bui4", "m_tier", 5);
        properties["m_count"] = Property("int", "m_count", 6, Dirty);
        Json mood = Property("enum TestMood", "m_mood", 7);
        mood["enum_options"] = Json{ { "kCalm", 0 }, { "kAngry", 4 } };
        properties["m_mood"] = mood;
        properties["m_title"] = Property("std::wstring", "m_title", 8);
        properties["m_offset"] = Property("bi5", "m_offset", 9);
        return properties;
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "enum TestMood", Json::array(), Json::object());
        AddClass(classes, "class Vector3D", Json::array(), Json::object());
        AddClass(classes, "class TestCard", Json::array({ "PropertyClass" }), CardProperties());
        Json rare = CardProperties();
        rare["m_rarity"] = Property("unsigned char", "m_rarity", 10);
        AddClass(classes, "class TestRareCard", Json::array({ "TestCard", "PropertyClass" }), rare);
        Json deck = Json::object();
        deck["m_cards"] = Property("class SharedPointer<class TestCard>", "m_cards", 0, Wire, "List");
        deck["m_leader"] = Property("class TestCard*", "m_leader", 1);
        deck["m_position"] = Property("class Vector3D", "m_position", 2);
        deck["m_cover"] = Property("class TestCard", "m_cover", 3);
        AddClass(classes, "class TestDeck", Json::array({ "PropertyClass" }), deck);
        AddClass(classes, "class TestCard*", Json::array({ "PropertyClass" }), CardProperties());

        for (char const* name : { "class Color", "class Euler", "enum TestMask" })
            AddClass(classes, name, Json::array(), Json::object());
        Json gadget = Json::object();
        gadget["m_tint"] = Property("class Color", "m_tint", 0);
        gadget["m_facing"] = Property("class Euler", "m_facing", 1);
        gadget["m_guid"] = Property("gid", "m_guid", 2);
        gadget["m_wide"] = Property("s24", "m_wide", 3);
        gadget["m_unsigned"] = Property("u24", "m_unsigned", 4);
        gadget["m_letter"] = Property("wchar_t", "m_letter", 5);
        gadget["m_shorts"] = Property("unsigned short", "m_shorts", 6, Wire, "List");
        gadget["m_bytes"] = Property("unsigned char", "m_bytes", 7, Wire, "Vector");
        Json mask = Property("enum TestMask", "m_mask", 8, WireBits);
        mask["enum_options"] = Json{ { "A", 1 }, { "B", 2 } };
        gadget["m_mask"] = mask;
        AddClass(classes, "class TestGadget", Json::array({ "PropertyClass" }), gadget);

        Json node = Json::object();
        node["m_next"] = Property("class TestNode*", "m_next", 0);
        AddClass(classes, "class TestNode", Json::array({ "PropertyClass" }), node);

        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "codec.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    void AppendU32(std::vector<uint8>& bytes, uint32 value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<uint8>(value >> shift));
    }

    void Append(std::vector<uint8>& bytes, std::initializer_list<uint8> values)
    {
        bytes.insert(bytes.end(), values);
    }

    class CompactCodecTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
        }

        PropertyObjectPtr MakeCard(std::string const& className, uint32 id)
        {
            PropertyObjectPtr card = PropertyObject::Create(_catalog, className);
            if (!card)
            {
                ADD_FAILURE() << className << " did not build";
                return card;
            }
            EXPECT_EQ(card->Set("m_id", id), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_secret", int32{ 99 }), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_old", 5.0f), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_name", "Ace"), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_shiny", true), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_tier", uint32{ 9 }), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_count", int32{ 7 }), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_mood", int64{ 4 }), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_title", u"Hi"), PropertySetResult::Ok);
            EXPECT_EQ(card->Set("m_offset", int32{ -3 }), PropertySetResult::Ok);
            return card;
        }

        std::vector<uint8> CardBytes(uint32 id, bool countPresent)
        {
            std::vector<uint8> bytes;
            AppendU32(bytes, StringHash::KiStringHash("class TestCard"));
            AppendU32(bytes, id);
            Append(bytes, { 0x03, 0x00, 'A', 'c', 'e' });
            if (countPresent)
            {
                Append(bytes, { 0x33 });
                AppendU32(bytes, 7);
            }
            else
            {
                Append(bytes, { 0x13 });
            }
            AppendU32(bytes, 4);
            Append(bytes, { 0x02, 0x00, 'H', 0x00, 'i', 0x00, 0x1D });
            return bytes;
        }

        PropertyObjectPtr MakeDeck()
        {
            PropertyObjectPtr deck = PropertyObject::Create(_catalog, "class TestDeck");
            PropertyObjectPtr rare = MakeCard("class TestRareCard", 1);
            if (!deck || !rare)
            {
                ADD_FAILURE() << "the deck did not build";
                return nullptr;
            }
            PropertyValue::List cards;
            EXPECT_EQ(rare->Set("m_rarity", uint8{ 3 }), PropertySetResult::Ok);
            cards.emplace_back(std::move(rare));
            cards.emplace_back(PropertyObjectPtr());
            cards.emplace_back(MakeCard("class TestCard", 2));
            EXPECT_EQ(deck->Set("m_cards", std::move(cards)), PropertySetResult::Ok);
            EXPECT_EQ(deck->Set("m_position", PropertyTypes::Vector3D{ 1.0f, -2.0f, 0.5f }), PropertySetResult::Ok);
            return deck;
        }

        TypeCatalogPtr _catalog;
    };
}

TEST_F(CompactCodecTest, TheMaskSelectsPropertiesDeprecatedOnesAreSkippedAndBitsPack)
{
    PropertyObjectPtr const card = MakeCard("class TestCard", 0x11223344u);
    ASSERT_TRUE(card);
    EncodeResult const encoded = ObjectSerializer::EncodeCompact(card.get());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, CardBytes(0x11223344u, true));

    DecodeResult const decoded = ObjectSerializer::DecodeCompact(_catalog, encoded.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_TRUE(decoded.Object);
    EXPECT_EQ(decoded.BytesRead, encoded.Bytes.size());
    EXPECT_EQ(*decoded.Object->Get("m_id")->GetIf<uint32>(), 0x11223344u);
    EXPECT_EQ(*decoded.Object->Get("m_secret")->GetIf<int32>(), 0);
    EXPECT_EQ(*decoded.Object->Get("m_old")->GetIf<float>(), 0.0f);
    EXPECT_EQ(*decoded.Object->Get("m_tier")->GetIf<uint32>(), 9u);
    EXPECT_EQ(*decoded.Object->Get("m_offset")->GetIf<int32>(), -3);
    EXPECT_TRUE(*decoded.Object->Get("m_title")->GetIf<std::u16string>() == u"Hi");

    SerializerOptions publicView;
    publicView.Mask = SerializerOptions::PublicMask;
    EncodeResult const visible = ObjectSerializer::EncodeCompact(card.get(), publicView);
    ASSERT_TRUE(visible.Ok()) << visible.Detail;
    std::vector<uint8> expected;
    AppendU32(expected, StringHash::KiStringHash("class TestCard"));
    Append(expected, { 0x03, 0x00, 'A', 'c', 'e' });
    EXPECT_EQ(visible.Bytes, expected);
    DecodeResult const seen = ObjectSerializer::DecodeCompact(_catalog, visible.Bytes, publicView);
    ASSERT_TRUE(seen.Ok()) << seen.Detail;
    EXPECT_EQ(*seen.Object->Get("m_name")->GetIf<std::string>(), "Ace");
    EXPECT_EQ(*seen.Object->Get("m_id")->GetIf<uint32>(), 0u);
}

TEST_F(CompactCodecTest, DirtyEncodedPropertiesCarryAPresentBit)
{
    PropertyObjectPtr const card = MakeCard("class TestCard", 5);
    ASSERT_TRUE(card);
    SerializerOptions clean;
    clean.IsDirty = [](PropertyObject const&, PropertyInfo const& property) { return property.Name != "m_count"; };
    EncodeResult const encoded = ObjectSerializer::EncodeCompact(card.get(), clean);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, CardBytes(5, false));

    DecodeResult const decoded = ObjectSerializer::DecodeCompact(_catalog, encoded.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(*decoded.Object->Get("m_count")->GetIf<int32>(), 0);
    EXPECT_EQ(*decoded.Object->Get("m_mood")->GetIf<int64>(), 4);

    clean.Flags = SerializerFlag::ForceDirtyEncode;
    EncodeResult const forced = ObjectSerializer::EncodeCompact(card.get(), clean);
    ASSERT_TRUE(forced.Ok()) << forced.Detail;
    EXPECT_EQ(forced.Bytes, CardBytes(5, true));
}

TEST_F(CompactCodecTest, ANestedListOfDerivedAndNullChildrenRoundTripsExactly)
{
    PropertyObjectPtr const deck = MakeDeck();
    ASSERT_TRUE(deck);
    EncodeResult const encoded = ObjectSerializer::EncodeCompact(deck.get());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    std::vector<uint8> prefix;
    AppendU32(prefix, StringHash::KiStringHash("class TestDeck"));
    AppendU32(prefix, 3);
    AppendU32(prefix, StringHash::KiStringHash("class TestRareCard"));
    ASSERT_GE(encoded.Bytes.size(), prefix.size());
    EXPECT_TRUE(std::equal(prefix.begin(), prefix.end(), encoded.Bytes.begin()));

    DecodeResult const decoded = ObjectSerializer::DecodeCompact(_catalog, encoded.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    PropertyValue::List const& cards = *decoded.Object->Get("m_cards")->GetList();
    ASSERT_EQ(cards.size(), 3u);
    ASSERT_NE(cards[0].AsObject(), nullptr);
    EXPECT_TRUE(cards[0].AsObject()->IsA("class TestRareCard"));
    EXPECT_EQ(*cards[0].AsObject()->Get("m_rarity")->GetIf<uint8>(), 3u);
    EXPECT_TRUE(cards[1].IsNullObject());
    EXPECT_FALSE(cards[2].AsObject()->IsA("class TestRareCard"));
    EXPECT_TRUE(decoded.Object->Get("m_leader")->IsNullObject());
    ASSERT_NE(decoded.Object->Get("m_cover")->AsObject(), nullptr);

    PropertyObjectPtr expected = deck->Clone();
    for (std::size_t index : { std::size_t{ 0 }, std::size_t{ 2 } })
    {
        PropertyObject* const card = expected->EditObjectAt(0, index);
        ASSERT_NE(card, nullptr);
        ASSERT_EQ(card->Set("m_secret", int32{ 0 }), PropertySetResult::Ok);
        ASSERT_EQ(card->Set("m_old", 0.0f), PropertySetResult::Ok);
    }
    EXPECT_TRUE(*decoded.Object == *expected);
    EncodeResult const again = ObjectSerializer::EncodeCompact(decoded.Object.get());
    ASSERT_TRUE(again.Ok()) << again.Detail;
    EXPECT_EQ(again.Bytes, encoded.Bytes);

    EncodeResult const nothing = ObjectSerializer::EncodeCompact(nullptr);
    ASSERT_TRUE(nothing.Ok());
    EXPECT_EQ(nothing.Bytes, (std::vector<uint8>{ 0, 0, 0, 0 }));
    DecodeResult const empty = ObjectSerializer::DecodeCompact(_catalog, nothing.Bytes);
    EXPECT_TRUE(empty.Ok());
    EXPECT_FALSE(empty.Object);
}

TEST_F(CompactCodecTest, ChildrenMustBeOfTheirPropertysClassAndInlineOnesCannotBeNull)
{
    std::vector<uint8> wrongClass;
    AppendU32(wrongClass, StringHash::KiStringHash("class TestDeck"));
    AppendU32(wrongClass, 1);
    AppendU32(wrongClass, StringHash::KiStringHash("class TestDeck"));
    DecodeResult const wrong = ObjectSerializer::DecodeCompact(_catalog, wrongClass);
    EXPECT_EQ(wrong.Status, SerializerStatus::WrongClass);
    EXPECT_EQ(wrong.Detail, "class TestDeck.m_cards[0] holds a class TestDeck, which is not a class TestCard");
    EXPECT_FALSE(wrong.Object);

    PropertyObjectPtr const deck = MakeDeck();
    ASSERT_TRUE(deck);
    std::vector<uint8> bytes = ObjectSerializer::EncodeCompact(deck.get()).Bytes;
    std::vector<uint8> cover;
    AppendU32(cover, StringHash::KiStringHash("class TestCard"));
    PropertyObjectPtr const blank = PropertyObject::Create(_catalog, "class TestCard");
    ASSERT_TRUE(blank);
    std::size_t const coverSize = ObjectSerializer::EncodeCompact(blank.get()).Bytes.size();
    ASSERT_EQ(coverSize, 22u);
    ASSERT_GE(bytes.size(), coverSize);
    std::size_t const coverAt = bytes.size() - coverSize;
    ASSERT_TRUE(std::equal(cover.begin(), cover.end(), bytes.begin() + static_cast<std::ptrdiff_t>(coverAt)));
    bytes.resize(coverAt);
    AppendU32(bytes, 0);
    DecodeResult const nullCover = ObjectSerializer::DecodeCompact(_catalog, bytes);
    EXPECT_EQ(nullCover.Status, SerializerStatus::NullNotAllowed);
    EXPECT_EQ(nullCover.Detail, "class TestDeck.m_cover holds a null inline object");
}

TEST_F(CompactCodecTest, MalformedOrHostileDataIsRefusedWithWhereItFailed)
{
    PropertyObjectPtr const deck = MakeDeck();
    ASSERT_TRUE(deck);
    std::vector<uint8> const bytes = ObjectSerializer::EncodeCompact(deck.get()).Bytes;
    for (std::size_t length = 0; length < bytes.size(); ++length)
    {
        DecodeResult const cut = ObjectSerializer::DecodeCompact(_catalog, std::span<uint8 const>(bytes.data(), length));
        EXPECT_EQ(cut.Status, SerializerStatus::Truncated) << "length " << length;
        EXPECT_FALSE(cut.Object);
    }

    std::vector<uint8> trailing = bytes;
    trailing.push_back(0);
    EXPECT_EQ(ObjectSerializer::DecodeCompact(_catalog, trailing).Status, SerializerStatus::TrailingBytes);
    SerializerOptions lenient;
    lenient.AllowTrailingBytes = true;
    DecodeResult const tolerated = ObjectSerializer::DecodeCompact(_catalog, trailing, lenient);
    EXPECT_TRUE(tolerated.Ok());
    EXPECT_EQ(tolerated.BytesRead, bytes.size());

    std::vector<uint8> unknown;
    AppendU32(unknown, 12345);
    DecodeResult const missing = ObjectSerializer::DecodeCompact(_catalog, unknown);
    EXPECT_EQ(missing.Status, SerializerStatus::UnknownClass);
    EXPECT_EQ(missing.Detail, "the object names class hash 12345, which the type dump does not list");
    std::vector<uint8> enumRoot;
    AppendU32(enumRoot, StringHash::KiStringHash("enum TestMood"));
    EXPECT_EQ(ObjectSerializer::DecodeCompact(_catalog, enumRoot).Status, SerializerStatus::NotAPropertyClass);

    std::vector<uint8> huge;
    AppendU32(huge, StringHash::KiStringHash("class TestDeck"));
    AppendU32(huge, 0x7FFFFFFFu);
    Append(huge, { 0, 0 });
    ASSERT_EQ(huge.size(), 10u);
    SerializerOptions unlimited;
    unlimited.Limits.MaxContainerCount = 0xFFFFFFFFu;
    DecodeResult const bomb = ObjectSerializer::DecodeCompact(_catalog, huge, unlimited);
    EXPECT_EQ(bomb.Status, SerializerStatus::Truncated);
    EXPECT_EQ(bomb.Detail, "class TestDeck.m_cards lists 2147483647 elements, more than the 2 bytes left can hold");
    EXPECT_EQ(ObjectSerializer::DecodeCompact(_catalog, huge).Status, SerializerStatus::ContainerTooLarge);

    SerializerOptions tight;
    tight.Limits.MaxContainerCount = 2;
    EXPECT_EQ(ObjectSerializer::DecodeCompact(_catalog, bytes, tight).Status, SerializerStatus::ContainerTooLarge);
    tight = {};
    tight.Limits.MaxDepth = 1;
    DecodeResult const deep = ObjectSerializer::DecodeCompact(_catalog, bytes, tight);
    EXPECT_EQ(deep.Status, SerializerStatus::TooDeep);
    EXPECT_EQ(deep.Detail, "class TestDeck.m_cards[0] nests objects deeper than 1");
    EXPECT_EQ(ObjectSerializer::EncodeCompact(deck.get(), tight).Status, SerializerStatus::TooDeep);
    tight = {};
    tight.Limits.MaxObjects = 3;
    EXPECT_EQ(ObjectSerializer::DecodeCompact(_catalog, bytes, tight).Status, SerializerStatus::TooManyObjects);

    SerializerOptions compactLength;
    compactLength.Flags = SerializerFlag::CompactLength;
    EXPECT_EQ(ObjectSerializer::DecodeCompact(_catalog, bytes, compactLength).Status, SerializerStatus::UnsupportedFlags);
    EXPECT_EQ(ObjectSerializer::EncodeCompact(deck.get(), compactLength).Status, SerializerStatus::UnsupportedFlags);

    PropertyObjectPtr const card = MakeCard("class TestCard", 1);
    ASSERT_TRUE(card);
    ASSERT_EQ(card->Set("m_name", std::string(70000, 'x')), PropertySetResult::Ok);
    EncodeResult const tooLong = ObjectSerializer::EncodeCompact(card.get());
    EXPECT_EQ(tooLong.Status, SerializerStatus::ValueTooLong);
    EXPECT_TRUE(tooLong.Bytes.empty());
}

TEST_F(CompactCodecTest, TextEnumsWriteOptionNames)
{
    PropertyObjectPtr const card = MakeCard("class TestCard", 1);
    ASSERT_TRUE(card);
    SerializerOptions text;
    text.Flags = SerializerFlag::StringEnums;
    EncodeResult const encoded = ObjectSerializer::EncodeCompact(card.get(), text);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    std::string const named(encoded.Bytes.begin(), encoded.Bytes.end());
    EXPECT_NE(named.find(std::string("\x06\x00kAngry", 8)), std::string::npos);
    DecodeResult const decoded = ObjectSerializer::DecodeCompact(_catalog, encoded.Bytes, text);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_TRUE(*decoded.Object == *ObjectSerializer::DecodeCompact(_catalog, ObjectSerializer::EncodeCompact(card.get()).Bytes).Object);

    std::vector<uint8> bytes = encoded.Bytes;
    std::size_t const at = named.find("kAngry");
    bytes[at] = 'x';
    DecodeResult const bad = ObjectSerializer::DecodeCompact(_catalog, bytes, text);
    EXPECT_EQ(bad.Status, SerializerStatus::UnknownEnumName);
    EXPECT_EQ(bad.Detail, "class TestCard.m_mood holds 'xAngry', which names no option");
}

TEST_F(CompactCodecTest, EveryOtherValueLayoutMatchesGoldenBytes)
{
    PropertyObjectPtr const gadget = PropertyObject::Create(_catalog, "class TestGadget");
    ASSERT_TRUE(gadget);
    ASSERT_EQ(gadget->Set("m_tint", PropertyTypes::Color{ 1, 2, 3, 4 }), PropertySetResult::Ok);
    ASSERT_EQ(gadget->Set("m_facing", PropertyTypes::Euler{ 1.0f, 0.5f, -2.0f }), PropertySetResult::Ok);
    ASSERT_EQ(gadget->Set("m_guid", uint64{ 0x0102030405060708u }), PropertySetResult::Ok);
    ASSERT_EQ(gadget->Set("m_wide", int32{ -2 }), PropertySetResult::Ok);
    ASSERT_EQ(gadget->Set("m_unsigned", uint32{ 0x123456 }), PropertySetResult::Ok);
    ASSERT_EQ(gadget->Set("m_letter", char16_t{ 0x5A }), PropertySetResult::Ok);
    PropertyValue::List shorts;
    shorts.emplace_back(uint16{ 0x1234 });
    shorts.emplace_back(uint16{ 0xABCD });
    ASSERT_EQ(gadget->Set("m_shorts", std::move(shorts)), PropertySetResult::Ok);
    PropertyValue::List bytes;
    for (uint8 value : { uint8{ 7 }, uint8{ 8 }, uint8{ 9 } })
        bytes.emplace_back(value);
    ASSERT_EQ(gadget->Set("m_bytes", std::move(bytes)), PropertySetResult::Ok);
    ASSERT_EQ(gadget->Set("m_mask", int64{ 3 }), PropertySetResult::Ok);

    std::vector<uint8> expected;
    AppendU32(expected, StringHash::KiStringHash("class TestGadget"));
    Append(expected, { 0x01, 0x02, 0x03, 0x04 });
    Append(expected, { 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0xC0 });
    Append(expected, { 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 });
    Append(expected, { 0xFE, 0xFF, 0xFF, 0x56, 0x34, 0x12, 0x5A, 0x00 });
    AppendU32(expected, 2);
    Append(expected, { 0x34, 0x12, 0xCD, 0xAB });
    AppendU32(expected, 3);
    Append(expected, { 0x07, 0x08, 0x09 });
    std::vector<uint8> named = expected;
    AppendU32(expected, 3);
    Append(named, { 0x03, 0x00, 'A', '|', 'B' });

    EncodeResult const encoded = ObjectSerializer::EncodeCompact(gadget.get());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, expected);
    DecodeResult const decoded = ObjectSerializer::DecodeCompact(_catalog, encoded.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_TRUE(*decoded.Object == *gadget);

    SerializerOptions text;
    text.Flags = SerializerFlag::StringEnums;
    EncodeResult const flagNames = ObjectSerializer::EncodeCompact(gadget.get(), text);
    ASSERT_TRUE(flagNames.Ok()) << flagNames.Detail;
    EXPECT_EQ(flagNames.Bytes, named);
    DecodeResult const fromNames = ObjectSerializer::DecodeCompact(_catalog, flagNames.Bytes, text);
    ASSERT_TRUE(fromNames.Ok()) << fromNames.Detail;
    EXPECT_TRUE(*fromNames.Object == *gadget);

    for (int64 const value : { int64{ 0 }, int64{ 8 } })
    {
        ASSERT_EQ(gadget->Set("m_mask", int64{ value }), PropertySetResult::Ok);
        EncodeResult const odd = ObjectSerializer::EncodeCompact(gadget.get(), text);
        ASSERT_TRUE(odd.Ok()) << odd.Detail;
        std::vector<uint8> tail(odd.Bytes.end() - (value == 0 ? 2 : 3), odd.Bytes.end());
        EXPECT_EQ(tail, value == 0 ? (std::vector<uint8>{ 0x00, 0x00 }) : (std::vector<uint8>{ 0x01, 0x00, '8' }));
        DecodeResult const back = ObjectSerializer::DecodeCompact(_catalog, odd.Bytes, text);
        ASSERT_TRUE(back.Ok()) << back.Detail;
        EXPECT_TRUE(*back.Object == *gadget);
    }
}

TEST_F(CompactCodecTest, AliasHashesDecodeToTheirClassAndReencodePlain)
{
    PropertyObjectPtr const card = MakeCard("class TestCard", 9);
    ASSERT_TRUE(card);
    std::vector<uint8> const plain = CardBytes(9, true);
    std::vector<uint8> aliased = plain;
    uint32 const alias = StringHash::KiStringHash("class TestCard*");
    for (int shift = 0; shift < 4; ++shift)
        aliased[static_cast<std::size_t>(shift)] = static_cast<uint8>(alias >> (8 * shift));
    DecodeResult const decoded = ObjectSerializer::DecodeCompact(_catalog, aliased);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(&decoded.Object->GetClass(), _catalog->FindClass("class TestCard"));
    EncodeResult const encoded = ObjectSerializer::EncodeCompact(decoded.Object.get());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, plain);
}

TEST_F(CompactCodecTest, RootsCanBeHeldToAllowedClassesAndRequiredToBePresent)
{
    PropertyObjectPtr const deck = MakeDeck();
    PropertyObjectPtr const rare = MakeCard("class TestRareCard", 2);
    ASSERT_TRUE(deck && rare);
    SerializerOptions cardsOnly;
    cardsOnly.RootClasses = { _catalog->FindClass("class TestCard") };
    DecodeResult const refused = ObjectSerializer::DecodeCompact(_catalog, ObjectSerializer::EncodeCompact(deck.get()).Bytes, cardsOnly);
    EXPECT_EQ(refused.Status, SerializerStatus::WrongClass);
    EXPECT_EQ(refused.Detail, "the object names class TestDeck, which is not a class allowed here");
    EXPECT_TRUE(ObjectSerializer::DecodeCompact(_catalog, ObjectSerializer::EncodeCompact(rare.get()).Bytes, cardsOnly).Ok());

    std::vector<uint8> const nothing{ 0, 0, 0, 0 };
    EXPECT_TRUE(ObjectSerializer::DecodeCompact(_catalog, nothing, cardsOnly).Ok());
    cardsOnly.AllowNullRoot = false;
    DecodeResult const missing = ObjectSerializer::DecodeCompact(_catalog, nothing, cardsOnly);
    EXPECT_EQ(missing.Status, SerializerStatus::NullNotAllowed);
    EXPECT_EQ(missing.Detail, "the object is null where an object is required");
}

TEST_F(CompactCodecTest, DecodesStayWithinTheMemoryBudgetAndTheDepthCeiling)
{
    PropertyObjectPtr const deck = MakeDeck();
    ASSERT_TRUE(deck);
    std::vector<uint8> const bytes = ObjectSerializer::EncodeCompact(deck.get()).Bytes;
    SerializerOptions small;
    small.Limits.MaxDecodedBytes = 512;
    DecodeResult const refused = ObjectSerializer::DecodeCompact(_catalog, bytes, small);
    EXPECT_EQ(refused.Status, SerializerStatus::BudgetExceeded);
    EXPECT_FALSE(refused.Object);
    EXPECT_NE(refused.Detail.find("needs more than the 512 bytes of memory a decode may use"), std::string::npos) << refused.Detail;

    ClassInfo const& card = *_catalog->FindClass("class TestCard");
    EXPECT_EQ(card.DefaultBytes, sizeof(PropertyObject) + card.Properties.size() * sizeof(PropertyValue));
    EXPECT_EQ(_catalog->FindClass("class TestDeck")->Properties[3].DefaultBytes, card.DefaultBytes);

    auto const chain = [](uint32 length)
    {
        std::vector<uint8> blob;
        for (uint32 index = 0; index < length; ++index)
            AppendU32(blob, StringHash::KiStringHash("class TestNode"));
        AppendU32(blob, 0);
        return blob;
    };
    SerializerOptions deep;
    deep.Limits.MaxDepth = 100000;
    EXPECT_TRUE(ObjectSerializer::DecodeCompact(_catalog, chain(SerializerLimits::DepthCeiling), deep).Ok());
    DecodeResult const tooDeep = ObjectSerializer::DecodeCompact(_catalog, chain(SerializerLimits::DepthCeiling + 1), deep);
    EXPECT_EQ(tooDeep.Status, SerializerStatus::TooDeep);
    EXPECT_NE(tooDeep.Detail.find("nests objects deeper than 128"), std::string::npos) << tooDeep.Detail;
}
