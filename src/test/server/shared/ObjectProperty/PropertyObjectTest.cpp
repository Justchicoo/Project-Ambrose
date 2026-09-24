/*
 * Project Ambrose by Imjustchico
 * Tests dynamic property objects on a small dump written by the test: defaults resolved from each form the dump writes them in, inline children created and pointers left null, classes and children from another catalog refused, a list of derived child objects passing IsA with a deep clone that equals the original exactly and stays independent, writes of the wrong kind, width, range, class, nullability or container refused without changing the object, an object never made to own itself, child objects reachable only as const through a const value, list elements and child objects edited in place under the same checks, lookups by name, hash and ordinal, and enum and Bits values, multi-bit and duplicate options included, rendered as option names and parsed back.
 */

#include "PropertyEnums.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <limits>
#include <optional>
#include <string>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Transmit = 31;
    constexpr uint32 BitsFlag = Transmit | (uint32{ 1 } << 20);

    template<typename T>
    concept ConstGettable = requires(PropertyValue const& value) { value.GetIf<T>(); };

    static_assert(ConstGettable<int32>);
    static_assert(ConstGettable<PropertyValue::List>);
    static_assert(!ConstGettable<PropertyObjectPtr>);

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", uint32 flags = Transmit)
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    Json WithDefault(Json property, Json value)
    {
        property["enum_options"] = Json{ { "__DEFAULT", std::move(value) } };
        return property;
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    Json ItemProperties()
    {
        Json properties = Json::object();
        properties["m_id"] = Property("unsigned __int64", "m_id", 0);
        properties["m_name"] = WithDefault(Property("std::string", "m_name", 1), 0);
        properties["m_weight"] = WithDefault(Property("float", "m_weight", 2), "2.5");
        properties["m_count"] = WithDefault(Property("int", "m_count", 3), 7);
        Json mood = Property("enum TestMood", "m_mood", 4);
        mood["enum_options"] = Json{ { "kCalm", 0 }, { "kAngry", 4 }, { "kUnknown", int64{ 4294967295 } }, { "__DEFAULT", "kAngry" } };
        properties["m_mood"] = mood;
        Json flags = Property("unsigned int", "m_flags", 5, "Static", BitsFlag);
        flags["enum_options"] = Json{ { "A", 1 }, { "AB", 3 }, { "Also", 1 }, { "B", 2 }, { "C", 4 }, { "HIGH", 48 }, { "__DEFAULT", "A|C" } };
        properties["m_flags"] = flags;
        properties["m_bits"] = Property("bui4", "m_bits", 6);
        properties["m_position"] = Property("class Vector3D", "m_position", 7);
        properties["m_children"] = Property("class SharedPointer<class TestItem>", "m_children", 8, "List");
        properties["m_owner"] = Property("class TestItem*", "m_owner", 9);
        properties["m_extra"] = Property("class TestExtra", "m_extra", 10);
        properties["m_label"] = WithDefault(Property("std::wstring", "m_label", 11), "\xC3\xA9t\xC3\xA9");
        properties["m_offset"] = WithDefault(Property("int", "m_offset", 12), int64{ 4294967295 });
        properties["m_small"] = WithDefault(Property("bi4", "m_small", 13), 15);
        properties["m_wide"] = WithDefault(Property("s24", "m_wide", 14), -5);
        properties["m_unsignedWide"] = WithDefault(Property("u24", "m_unsignedWide", 15), 16777215);
        properties["m_letter"] = WithDefault(Property("wchar_t", "m_letter", 16), 65);
        properties["m_guid"] = WithDefault(Property("gid", "m_guid", 17), "123456789012");
        properties["m_enabled"] = WithDefault(Property("bool", "m_enabled", 18), 1);
        properties["m_ratio"] = WithDefault(Property("double", "m_ratio", 19), "0.25");
        properties["m_level"] = WithDefault(Property("char", "m_level", 20), "-1");
        return properties;
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "enum TestMood", Json::array(), Json::object());
        AddClass(classes, "class Vector3D", Json::array(), Json::object());
        Json extra = Json::object();
        extra["m_level"] = Property("unsigned char", "m_level", 0);
        AddClass(classes, "class TestExtra", Json::array({ "PropertyClass" }), extra);
        AddClass(classes, "class TestItem", Json::array({ "PropertyClass" }), ItemProperties());
        Json sword = ItemProperties();
        sword["m_sharpness"] = Property("float", "m_sharpness", 21);
        AddClass(classes, "class TestSword", Json::array({ "TestItem", "PropertyClass" }), sword);
        Json other = Json::object();
        other["m_value"] = Property("int", "m_value", 0);
        AddClass(classes, "class TestOther", Json::array({ "PropertyClass" }), other);

        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "objects.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    template<typename T>
    std::optional<T> ValueOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        T const* const typed = value ? value->GetIf<T>() : nullptr;
        return typed ? std::optional<T>(*typed) : std::nullopt;
    }

    class PropertyObjectTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
        }

        TypeCatalogPtr _catalog;
    };
}

TEST_F(PropertyObjectTest, NewObjectsTakeTheDumpsDefaults)
{
    PropertyObjectPtr const item = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(item);
    EXPECT_EQ(&item->GetClass(), _catalog->FindClass("class TestItem"));
    EXPECT_EQ(item->GetClass().Owner, _catalog.get());
    EXPECT_EQ(ValueOf<int32>(*item, "m_count"), 7);
    EXPECT_EQ(ValueOf<float>(*item, "m_weight"), 2.5f);
    EXPECT_EQ(ValueOf<int64>(*item, "m_mood"), 4);
    EXPECT_EQ(ValueOf<uint32>(*item, "m_flags"), 5u);
    EXPECT_EQ(ValueOf<int32>(*item, "m_offset"), -1);
    EXPECT_EQ(ValueOf<std::string>(*item, "m_name"), "");
    EXPECT_EQ(ValueOf<std::u16string>(*item, "m_label"), (std::u16string{ char16_t{ 0xE9 }, u't', char16_t{ 0xE9 } }));
    EXPECT_EQ(ValueOf<uint64>(*item, "m_id"), 0u);
    EXPECT_EQ(ValueOf<int32>(*item, "m_small"), -1);
    EXPECT_EQ(ValueOf<int32>(*item, "m_wide"), -5);
    EXPECT_EQ(ValueOf<uint32>(*item, "m_unsignedWide"), 16777215u);
    EXPECT_EQ(ValueOf<char16_t>(*item, "m_letter"), u'A');
    EXPECT_EQ(ValueOf<uint64>(*item, "m_guid"), 123456789012u);
    EXPECT_EQ(ValueOf<bool>(*item, "m_enabled"), true);
    EXPECT_EQ(ValueOf<double>(*item, "m_ratio"), 0.25);
    EXPECT_EQ(ValueOf<int8>(*item, "m_level"), int8{ -1 });
    EXPECT_EQ(ValueOf<PropertyTypes::Vector3D>(*item, "m_position"), PropertyTypes::Vector3D{});
    ASSERT_NE(item->Get("m_children")->GetList(), nullptr);
    EXPECT_TRUE(item->Get("m_children")->GetList()->empty());
    EXPECT_TRUE(item->Get("m_owner")->IsNullObject());
    PropertyObject const* const extra = item->Get("m_extra")->AsObject();
    ASSERT_NE(extra, nullptr);
    EXPECT_TRUE(extra->IsA("class TestExtra"));
    EXPECT_EQ(ValueOf<uint8>(*extra, "m_level"), uint8{ 0 });

    EXPECT_EQ(PropertyObject::Create(_catalog, "class TestMissing"), nullptr);
    EXPECT_EQ(PropertyObject::Create(_catalog, "enum TestMood"), nullptr);
    EXPECT_EQ(PropertyObject::Create(nullptr, "class TestItem"), nullptr);
    TypeCatalogPtr const other = LoadCatalog();
    ASSERT_TRUE(other);
    EXPECT_EQ(PropertyObject::Create(_catalog, *other->FindClass("class TestItem")), nullptr);
}

TEST_F(PropertyObjectTest, AListOfDerivedChildrenPassesIsAAndACloneEqualsTheOriginal)
{
    PropertyObjectPtr item = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(item);
    PropertyValue::List children;
    for (int i = 0; i < 2; ++i)
    {
        PropertyObjectPtr sword = PropertyObject::Create(_catalog, "class TestSword");
        ASSERT_TRUE(sword);
        ASSERT_EQ(sword->Set("m_sharpness", 1.5f + static_cast<float>(i)), PropertySetResult::Ok);
        ASSERT_EQ(sword->Set("m_name", "Blade"), PropertySetResult::Ok);
        children.emplace_back(std::move(sword));
    }
    children.emplace_back(PropertyObjectPtr());
    ASSERT_EQ(item->Set("m_children", std::move(children)), PropertySetResult::Ok);
    ASSERT_EQ(item->Set("m_owner", PropertyObject::Create(_catalog, "class TestSword")), PropertySetResult::Ok);

    PropertyValue::List const& stored = *item->Get("m_children")->GetList();
    ASSERT_EQ(stored.size(), 3u);
    ClassInfo const& itemClass = *_catalog->FindClass("class TestItem");
    for (std::size_t i = 0; i < 2; ++i)
    {
        PropertyObject const* const child = stored[i].AsObject();
        ASSERT_NE(child, nullptr);
        EXPECT_TRUE(child->IsA(itemClass));
        EXPECT_TRUE(child->IsA("class TestSword"));
        EXPECT_FALSE(child->IsA("class TestOther"));
    }
    EXPECT_TRUE(stored[2].IsNullObject());

    PropertyObjectPtr clone = item->Clone();
    ASSERT_TRUE(clone);
    EXPECT_TRUE(*clone == *item);
    EXPECT_NE(clone->Get("m_children")->GetList()->front().AsObject(), stored.front().AsObject());

    PropertyValue changedChildren = *clone->Get("m_children");
    ASSERT_EQ(changedChildren.GetIf<PropertyValue::List>()->front().AsObject()->Set("m_sharpness", 9.0f), PropertySetResult::Ok);
    ASSERT_EQ(clone->Set("m_children", std::move(changedChildren)), PropertySetResult::Ok);
    EXPECT_FALSE(*clone == *item);
    EXPECT_EQ(ValueOf<float>(*stored.front().AsObject(), "m_sharpness"), 1.5f);

    PropertyObjectPtr odd = PropertyObject::Create(_catalog, "class TestSword");
    ASSERT_TRUE(odd);
    ASSERT_EQ(odd->Set("m_sharpness", std::numeric_limits<float>::quiet_NaN()), PropertySetResult::Ok);
    ASSERT_EQ(odd->Set("m_position", PropertyTypes::Vector3D{ 1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f }), PropertySetResult::Ok);
    EXPECT_TRUE(*odd->Clone() == *odd);
    PropertyObjectPtr positive = odd->Clone();
    PropertyObjectPtr negative = odd->Clone();
    ASSERT_EQ(positive->Set("m_sharpness", 0.0f), PropertySetResult::Ok);
    ASSERT_EQ(negative->Set("m_sharpness", -0.0f), PropertySetResult::Ok);
    EXPECT_FALSE(*positive == *negative);
}

TEST_F(PropertyObjectTest, WritesOfTheWrongKindWidthClassOrContainerAreRefused)
{
    PropertyObjectPtr item = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(item);
    EXPECT_EQ(item->Set("m_weight", "heavy"), PropertySetResult::WrongKind);
    EXPECT_EQ(item->Set("m_weight", 3.0), PropertySetResult::WrongKind);
    EXPECT_EQ(item->Set("m_count", int64{ 3 }), PropertySetResult::WrongKind);
    EXPECT_EQ(item->Set("m_bits", uint32{ 16 }), PropertySetResult::OutOfRange);
    EXPECT_EQ(item->Set("m_bits", uint32{ 15 }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_small", int32{ 8 }), PropertySetResult::OutOfRange);
    EXPECT_EQ(item->Set("m_small", int32{ -9 }), PropertySetResult::OutOfRange);
    EXPECT_EQ(item->Set("m_small", int32{ -8 }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_wide", int32{ 1 << 23 }), PropertySetResult::OutOfRange);
    EXPECT_EQ(item->Set("m_unsignedWide", uint32{ 1 } << 24), PropertySetResult::OutOfRange);
    EXPECT_EQ(item->Set("m_owner", PropertyObject::Create(_catalog, "class TestOther")), PropertySetResult::WrongClass);
    EXPECT_EQ(item->Set("m_extra", PropertyObjectPtr()), PropertySetResult::NullNotAllowed);
    EXPECT_EQ(item->Set("m_extra", PropertyObject::Create(_catalog, "class TestItem")), PropertySetResult::WrongClass);
    TypeCatalogPtr const other = LoadCatalog();
    ASSERT_TRUE(other);
    EXPECT_EQ(item->Set("m_owner", PropertyObject::Create(other, "class TestItem")), PropertySetResult::OtherCatalog);
    EXPECT_EQ(item->Set("m_owner", PropertyObject::Create(item->GetCatalog(), "class TestSword")), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_children", PropertyObject::Create(_catalog, "class TestItem")), PropertySetResult::WrongKind);

    EXPECT_EQ(item->Set("m_mood", int32{ 4 }), PropertySetResult::WrongKind);
    EXPECT_EQ(item->Set("m_mood", int64{ -1 }), PropertySetResult::OutOfRange);
    EXPECT_EQ(item->Set("m_mood", int64{ 1 } << 32), PropertySetResult::OutOfRange);
    EXPECT_EQ(item->Set("m_mood", int64{ 4294967295 }), PropertySetResult::Ok);

    PropertyValue::List kept;
    kept.emplace_back(PropertyObject::Create(_catalog, "class TestSword"));
    ASSERT_EQ(item->Set("m_children", std::move(kept)), PropertySetResult::Ok);
    PropertyValue::List mixed;
    mixed.emplace_back(PropertyObject::Create(_catalog, "class TestItem"));
    mixed.emplace_back(int32{ 5 });
    EXPECT_EQ(item->Set("m_children", std::move(mixed)), PropertySetResult::WrongKind);
    ASSERT_EQ(item->Get("m_children")->GetList()->size(), 1u);
    EXPECT_TRUE(item->Get("m_children")->GetList()->front().AsObject()->IsA("class TestSword"));

    EXPECT_EQ(item->Set("m_unknown", int32{ 1 }), PropertySetResult::UnknownProperty);
    EXPECT_EQ(item->SetAt(99, int32{ 1 }), PropertySetResult::UnknownProperty);
    EXPECT_EQ(ValueOf<float>(*item, "m_weight"), 2.5f);

    EXPECT_EQ(item->Set(StringHash::PropertyHash("std::wstring", "m_label"), u"Wand"), PropertySetResult::Ok);
    ASSERT_NE(item->GetAt(11), nullptr);
    EXPECT_EQ(item->GetAt(11)->GetIf<std::u16string>() ? *item->GetAt(11)->GetIf<std::u16string>() : std::u16string(), u"Wand");
    ASSERT_NE(item->Get(StringHash::PropertyHash("unsigned int", "m_flags")), nullptr);
    EXPECT_TRUE(item->Get(StringHash::PropertyHash("unsigned int", "m_flags"))->Holds<uint32>());
    EXPECT_EQ(item->Get("m_unknown"), nullptr);
    EXPECT_EQ(item->GetAt(99), nullptr);
    EXPECT_EQ(PropertyObject::GetResultName(PropertySetResult::WrongClass), "the object is not of the property's class");

    EXPECT_EQ(PropertyValue(static_cast<char const*>(nullptr)), PropertyValue(std::string()));
    EXPECT_EQ(PropertyValue(static_cast<char16_t const*>(nullptr)), PropertyValue(std::u16string()));
}

TEST_F(PropertyObjectTest, AnObjectCannotBeMadeToOwnItself)
{
    PropertyObjectPtr item = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(item);
    PropertyObject* const raw = item.get();
    PropertyValue self(std::move(item));
    EXPECT_EQ(raw->Set("m_owner", std::move(self)), PropertySetResult::WouldOwnItself);
    EXPECT_EQ(self.AsObject(), raw);

    PropertyObjectPtr parent = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(parent);
    ASSERT_EQ(parent->SetElementAt(8, 0, PropertyObject::Create(_catalog, "class TestSword")), PropertySetResult::Ok);
    PropertyObject* const child = parent->EditObjectAt(8, 0);
    ASSERT_NE(child, nullptr);
    PropertyObject* const parentRaw = parent.get();
    PropertyValue ancestor(std::move(parent));
    EXPECT_EQ(child->SetElementAt(8, 0, std::move(ancestor)), PropertySetResult::WouldOwnItself);
    EXPECT_EQ(ancestor.AsObject(), parentRaw);
    PropertyValue::List loop;
    loop.push_back(std::move(ancestor));
    PropertyValue looped(std::move(loop));
    EXPECT_EQ(child->Set("m_children", std::move(looped)), PropertySetResult::WouldOwnItself);
    ASSERT_NE(looped.GetList(), nullptr);
    EXPECT_EQ(looped.GetList()->front().AsObject(), parentRaw);

    PropertyValue refused("heavy");
    EXPECT_EQ(child->Set("m_weight", std::move(refused)), PropertySetResult::WrongKind);
    EXPECT_EQ(refused, PropertyValue("heavy"));
}

TEST_F(PropertyObjectTest, ListElementsAndChildObjectsAreEditedInPlaceUnderTheSameChecks)
{
    PropertyObjectPtr item = PropertyObject::Create(_catalog, "class TestItem");
    ASSERT_TRUE(item);
    std::size_t const children = item->GetClass().FindProperty("m_children")->Id;
    std::size_t const extra = item->GetClass().FindProperty("m_extra")->Id;
    std::size_t const owner = item->GetClass().FindProperty("m_owner")->Id;
    EXPECT_EQ(item->SetElementAt(children, 0, PropertyObject::Create(_catalog, "class TestSword")), PropertySetResult::Ok);
    EXPECT_EQ(item->SetElementAt(children, 1, PropertyObject::Create(_catalog, "class TestItem")), PropertySetResult::Ok);
    EXPECT_EQ(item->SetElementAt(children, 3, PropertyObject::Create(_catalog, "class TestItem")), PropertySetResult::NoSuchElement);
    EXPECT_EQ(item->SetElementAt(children, 2, PropertyObject::Create(_catalog, "class TestOther")), PropertySetResult::WrongClass);
    EXPECT_EQ(item->SetElementAt(children, 2, int32{ 1 }), PropertySetResult::WrongKind);
    EXPECT_EQ(item->SetElementAt(extra, 0, PropertyObject::Create(_catalog, "class TestExtra")), PropertySetResult::WrongKind);
    EXPECT_EQ(item->SetElementAt(99, 0, int32{ 1 }), PropertySetResult::UnknownProperty);
    ASSERT_EQ(item->Get("m_children")->GetList()->size(), 2u);

    PropertyObject* const sword = item->EditObjectAt(children, 0);
    ASSERT_NE(sword, nullptr);
    EXPECT_EQ(sword->Set("m_sharpness", 4.0f), PropertySetResult::Ok);
    EXPECT_EQ(ValueOf<float>(*item->Get("m_children")->GetList()->front().AsObject(), "m_sharpness"), 4.0f);
    EXPECT_EQ(item->EditObjectAt(children, 2), nullptr);
    PropertyObject* const inner = item->EditObjectAt(extra);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->Set("m_level", uint8{ 9 }), PropertySetResult::Ok);
    EXPECT_EQ(ValueOf<uint8>(*item->Get("m_extra")->AsObject(), "m_level"), uint8{ 9 });
    EXPECT_EQ(item->EditObjectAt(extra, 1), nullptr);
    EXPECT_EQ(item->EditObjectAt(owner), nullptr);
    EXPECT_EQ(item->EditObjectAt(item->GetClass().FindProperty("m_count")->Id), nullptr);
    EXPECT_EQ(item->EditObjectAt(99), nullptr);

    EXPECT_EQ(item->SetElementAt(children, 1, PropertyObjectPtr()), PropertySetResult::Ok);
    EXPECT_EQ(item->EditObjectAt(children, 1), nullptr);
    EXPECT_EQ(item->EraseElementAt(children, 0), PropertySetResult::Ok);
    ASSERT_EQ(item->Get("m_children")->GetList()->size(), 1u);
    EXPECT_TRUE(item->Get("m_children")->GetList()->front().IsNullObject());
    EXPECT_EQ(item->EraseElementAt(children, 1), PropertySetResult::NoSuchElement);
    EXPECT_EQ(item->EraseElementAt(extra, 0), PropertySetResult::WrongKind);
    EXPECT_EQ(item->EraseElementAt(99, 0), PropertySetResult::UnknownProperty);
}

TEST_F(PropertyObjectTest, EnumAndBitsValuesRenderAsNamesAndParseBack)
{
    ClassInfo const& itemClass = *_catalog->FindClass("class TestItem");
    PropertyInfo const& flags = *itemClass.FindProperty("m_flags");
    EXPECT_EQ(PropertyEnums::Format(flags, 5), "A|C");
    EXPECT_EQ(PropertyEnums::Parse(flags, "A|C"), 5);
    EXPECT_EQ(PropertyEnums::Parse(flags, " C | A "), 5);
    EXPECT_EQ(PropertyEnums::Format(flags, 2), "B");
    EXPECT_EQ(PropertyEnums::Format(flags, 3), "AB");
    EXPECT_EQ(PropertyEnums::Format(flags, 1), "A");
    EXPECT_EQ(PropertyEnums::Format(flags, 7), "A|B|C");
    EXPECT_EQ(PropertyEnums::Format(flags, 0x31), "A|HIGH");
    EXPECT_EQ(PropertyEnums::Parse(flags, "AB|HIGH"), 0x33);
    EXPECT_EQ(PropertyEnums::Parse(flags, "Also"), 1);
    EXPECT_EQ(PropertyEnums::Format(flags, 0), "");
    EXPECT_EQ(PropertyEnums::Parse(flags, ""), 0);
    EXPECT_FALSE(PropertyEnums::Format(flags, 8));
    EXPECT_FALSE(PropertyEnums::Format(flags, 0x11));
    EXPECT_FALSE(PropertyEnums::Parse(flags, "A|D"));
    EXPECT_FALSE(PropertyEnums::Parse(flags, "A||C"));
    EXPECT_EQ(PropertyEnums::Parse(flags, "A|8"), 9);

    PropertyInfo const& mood = *itemClass.FindProperty("m_mood");
    EXPECT_EQ(PropertyEnums::Format(mood, 4), "kAngry");
    EXPECT_FALSE(PropertyEnums::Format(mood, 5));
    EXPECT_EQ(PropertyEnums::Format(mood, 4294967295), "kUnknown");
    EXPECT_EQ(PropertyEnums::Format(mood, -1), "kUnknown");
    EXPECT_FALSE(PropertyEnums::Format(mood, int64{ 1 } << 32));
    EXPECT_EQ(PropertyEnums::Parse(mood, "kCalm"), 0);
    EXPECT_EQ(PropertyEnums::Parse(mood, "kUnknown"), 4294967295);
    EXPECT_EQ(PropertyEnums::Parse(mood, "-1"), 4294967295);
    EXPECT_EQ(PropertyEnums::Parse(mood, "3"), 3);
    EXPECT_FALSE(PropertyEnums::Parse(mood, "4294967296"));
    EXPECT_FALSE(PropertyEnums::Parse(mood, "kSleepy"));
    EXPECT_EQ(PropertyEnums::Normalize(-2), 4294967294);
    EXPECT_FALSE(PropertyEnums::Normalize(int64{ std::numeric_limits<int32>::min() } - 1));
}
