/*
 * Project Ambrose by Imjustchico
 * Tests typed views on classes the test invents: views bind when a dump loads and read fields, inherited ones through derived objects included, straight from the object's stored values by cached ordinal; a view naming a missing class or property, the wrong dump type or the wrong C++ type fails the load with a precise message; a dump whose derived class moves an inherited property or changes its container is refused; the view list closes once a dump loads; and a reload that drops a bound property keeps the old catalog while one that moves ordinals rebinds, leaving a view built before it reading the old object correctly after the old catalog is released.
 */

#include "StringHash.h"
#include "TypedView.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", 31 }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    enum class DumpChange
    {
        None,
        DropName,
        LeadingExtra,
        MoveInheritedId,
        ChangeInheritedContainer
    };

    std::string MakeDump(DumpChange change = DumpChange::None)
    {
        uint32 const first = change == DumpChange::LeadingExtra ? 1 : 0;
        bool const withName = change != DumpChange::DropName;
        Json item = Json::object();
        if (change == DumpChange::LeadingExtra)
            item["m_extra"] = Property("int", "m_extra", 0);
        item["m_id"] = Property("unsigned int", "m_id", first);
        item["m_count"] = Property("unsigned int", "m_count", first + 1);
        if (withName)
            item["m_name"] = Property("std::string", "m_name", first + 2);
        uint32 const next = first + (withName ? 3 : 2);
        item["m_children"] = Property("class SharedPointer<class TestItem>", "m_children", next, "List");
        item["m_owner"] = Property("class TestItem*", "m_owner", next + 1);

        Json sword = item;
        sword["m_sharpness"] = Property("float", "m_sharpness", next + 2);
        if (change == DumpChange::MoveInheritedId)
        {
            sword["m_id"]["id"] = next + 2;
            sword["m_sharpness"]["id"] = first;
        }
        if (change == DumpChange::ChangeInheritedContainer)
        {
            sword["m_id"]["container"] = "List";
            sword["m_id"]["dynamic"] = true;
        }

        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "class TestItem", Json::array({ "PropertyClass" }), item);
        AddClass(classes, "class TestSword", Json::array({ "TestItem", "PropertyClass" }), sword);
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }

    class TestItemView : public TypedView<TestItemView>
    {
    public:
        enum Field : std::size_t { Id, Name, Children, Owner, FieldCount };
        static constexpr std::array<ViewField, FieldCount> Fields{ {
            ViewField::Of<uint32>(Id, "unsigned int", "m_id"),
            ViewField::Of<std::string>(Name, "std::string", "m_name"),
            ViewField::Of<PropertyValue::List>(Children, "class SharedPointer<class TestItem>", "m_children"),
            ViewField::Of<PropertyObjectPtr>(Owner, "class TestItem*", "m_owner"),
        } };
        static constexpr ViewDefinition Definition{ "TestItemView", "class TestItem", Fields };

        decltype(auto) GetId() const noexcept { return Read<Id>(); }
        decltype(auto) GetName() const noexcept { return Read<Name>(); }
        decltype(auto) GetChildren() const noexcept { return Read<Children>(); }
        decltype(auto) GetOwner() const noexcept { return Read<Owner>(); }

        static TestItemView OverOrdinals(PropertyObject const& object, uint32 const* ordinals) noexcept
        {
            return TestItemView(object, ordinals);
        }

        AMBROSE_TYPED_VIEW(TestItemView)
    };

    class TestSwordView : public TypedView<TestSwordView>
    {
    public:
        enum Field : std::size_t { Sharpness, FieldCount };
        static constexpr std::array<ViewField, FieldCount> Fields{ { ViewField::Of<float>(Sharpness, "float", "m_sharpness") } };
        static constexpr ViewDefinition Definition{ "TestSwordView", "class TestSword", Fields };

        decltype(auto) GetSharpness() const noexcept { return Read<Sharpness>(); }

        AMBROSE_TYPED_VIEW(TestSwordView)
    };

    static_assert(std::is_same_v<decltype(std::declval<TestItemView const&>().GetId()), uint32 const&>);
    static_assert(std::is_same_v<decltype(std::declval<TestItemView const&>().GetChildren()), PropertyValue::List const&>);
    static_assert(std::is_same_v<decltype(std::declval<TestItemView const&>().GetOwner()), PropertyObject const*>);
    static_assert(!std::is_constructible_v<TestItemView, PropertyObject const&, uint32 const*>);

    constexpr std::array<ViewField, 1> MissingFields{ { ViewField::Of<uint32>(0, "unsigned int", "m_missing") } };
    constexpr ViewDefinition MissingPropertyView{ "TestMissingPropertyView", "class TestItem", MissingFields };
    constexpr std::array<ViewField, 1> WrongTypeFields{ { ViewField::Of<int32>(0, "int", "m_id") } };
    constexpr ViewDefinition WrongTypeView{ "TestWrongTypeView", "class TestItem", WrongTypeFields };
    constexpr std::array<ViewField, 1> WrongStorageFields{ { ViewField::Of<int32>(0, "unsigned int", "m_id") } };
    constexpr ViewDefinition WrongStorageView{ "TestWrongStorageView", "class TestItem", WrongStorageFields };
    constexpr ViewDefinition MissingClassView{ "TestMissingClassView", "class TestGhost", MissingFields };

    std::vector<std::string> Sorted(std::vector<std::string> values)
    {
        std::sort(values.begin(), values.end());
        return values;
    }
}

TEST(TypedViewTest, ViewsBindAtLoadAndReadStoredValuesByCachedOrdinal)
{
    TypedViewRegistry views;
    EXPECT_TRUE(views.Add(TestItemView::Definition));
    EXPECT_TRUE(views.Add(TestSwordView::Definition));
    EXPECT_TRUE(views.Add(TestItemView::Definition));
    ASSERT_EQ(views.GetViews().size(), 2u);
    TypeRegistry registry(&views);
    ASSERT_TRUE(registry.LoadFromText(MakeDump(), "views.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_EQ(catalog->GetViews().size(), 2u);
    ViewBinding const* const binding = catalog->FindView(TestItemView::Definition);
    ASSERT_NE(binding, nullptr);
    EXPECT_EQ(binding->Class, catalog->FindClass("class TestItem"));

    PropertyObjectPtr sword = PropertyObject::Create(catalog, "class TestSword");
    ASSERT_TRUE(sword);
    ASSERT_EQ(sword->Set("m_id", uint32{ 1652259 }), PropertySetResult::Ok);
    ASSERT_EQ(sword->Set("m_count", uint32{ 42 }), PropertySetResult::Ok);
    ASSERT_EQ(sword->Set("m_name", "Blade"), PropertySetResult::Ok);
    ASSERT_EQ(sword->Set("m_sharpness", 2.5f), PropertySetResult::Ok);
    ASSERT_EQ(sword->Set("m_owner", PropertyObject::Create(catalog, "class TestItem")), PropertySetResult::Ok);

    std::optional<TestItemView> const item = TestItemView::From(*sword);
    ASSERT_TRUE(item);
    EXPECT_EQ(item->GetId(), 1652259u);
    EXPECT_EQ(item->GetName(), "Blade");
    EXPECT_TRUE(item->GetChildren().empty());
    ASSERT_NE(item->GetOwner(), nullptr);
    EXPECT_TRUE(item->GetOwner()->IsA("class TestItem"));
    EXPECT_EQ(&item->Target(), sword.get());
    for (std::size_t field = 0; field < TestItemView::FieldCount; ++field)
        EXPECT_EQ(item->GetOrdinal(field), catalog->FindClass("class TestItem")->FindProperty(TestItemView::Fields[field].Hash)->Id);
    EXPECT_EQ(item->GetOrdinal(TestItemView::FieldCount), std::numeric_limits<uint32>::max());
    EXPECT_EQ(&item->GetName(), sword->GetAt(item->GetOrdinal(TestItemView::Name))->GetIf<std::string>());

    uint32 const countOrdinal = catalog->FindClass("class TestItem")->FindProperty("m_count")->Id;
    std::array<uint32, TestItemView::FieldCount> redirected{ countOrdinal, binding->Ordinals[1], binding->Ordinals[2], binding->Ordinals[3] };
    TestItemView const overCount = TestItemView::OverOrdinals(*sword, redirected.data());
    EXPECT_EQ(overCount.GetId(), 42u);

    std::optional<TestSwordView> const blade = TestSwordView::From(sword.get());
    ASSERT_TRUE(blade);
    EXPECT_EQ(blade->GetSharpness(), 2.5f);
    PropertyObjectPtr const plain = PropertyObject::Create(catalog, "class TestItem");
    EXPECT_FALSE(TestSwordView::From(*plain));
    EXPECT_TRUE(TestItemView::From(*plain));
    EXPECT_FALSE(TestItemView::From(nullptr));

    TypeRegistry unbound;
    ASSERT_TRUE(unbound.LoadFromText(MakeDump(), "unbound.json"));
    PropertyObjectPtr const stranger = PropertyObject::Create(unbound.GetCatalog(), "class TestItem");
    EXPECT_FALSE(TestItemView::From(*stranger));
}

TEST(TypedViewTest, AViewThatCannotBindFailsTheLoadPrecisely)
{
    TypedViewRegistry views;
    views.Add(TestItemView::Definition);
    views.Add(MissingPropertyView);
    views.Add(WrongTypeView);
    views.Add(WrongStorageView);
    views.Add(MissingClassView);
    TypeRegistry registry(&views);
    EXPECT_FALSE(registry.LoadFromText(MakeDump(), "broken.json"));
    EXPECT_FALSE(registry.GetCatalog());
    EXPECT_EQ(Sorted(registry.GetErrors()), Sorted({
        "view TestMissingPropertyView: class TestItem has no property m_missing",
        "view TestWrongTypeView: class TestItem property m_id has type unsigned int, not int",
        "view TestWrongStorageView: class TestItem property m_id of type unsigned int is not stored as the C++ type the field declares",
        "view TestMissingClassView names class TestGhost, which the type dump does not list as a property class" }));
    EXPECT_TRUE(views.IsSealed());
    EXPECT_FALSE(views.Add(TestSwordView::Definition));
    EXPECT_EQ(views.GetViews().size(), 5u);

    TypeRegistry moved;
    EXPECT_FALSE(moved.LoadFromText(MakeDump(DumpChange::MoveInheritedId), "moved.json"));
    ASSERT_FALSE(moved.GetErrors().empty());
    EXPECT_EQ(moved.GetErrors().front(), "class TestSword lists class TestItem's property m_id with id 5 instead of 0, but views and compact data rely on inherited properties keeping their id");

    TypeRegistry contained;
    EXPECT_FALSE(contained.LoadFromText(MakeDump(DumpChange::ChangeInheritedContainer), "container.json"));
    EXPECT_EQ(contained.GetErrors(), (std::vector<std::string>{ "class TestSword lists class TestItem's property m_id with container List instead of Static, but views rely on inherited properties keeping their layout" }));
}

TEST(TypedViewTest, AReloadThatBreaksAViewKeepsTheOldCatalogAndOneThatMovesOrdinalsRebinds)
{
    TypedViewRegistry views;
    views.Add(TestItemView::Definition);
    TypeRegistry registry(&views);
    ASSERT_TRUE(registry.LoadFromText(MakeDump(), "first.json"));
    TypeCatalogPtr first = registry.GetCatalog();
    PropertyObjectPtr old = PropertyObject::Create(first, "class TestItem");
    ASSERT_EQ(old->Set("m_name", "Old"), PropertySetResult::Ok);
    std::optional<TestItemView> const oldView = TestItemView::From(*old);
    ASSERT_TRUE(oldView);

    EXPECT_FALSE(registry.LoadFromText(MakeDump(DumpChange::DropName), "dropped.json"));
    EXPECT_EQ(registry.GetCatalog(), first);
    EXPECT_EQ(registry.GetErrors(), (std::vector<std::string>{ "view TestItemView: class TestItem has no property m_name" }));

    ASSERT_TRUE(registry.LoadFromText(MakeDump(DumpChange::LeadingExtra), "moved.json"));
    TypeCatalogPtr const second = registry.GetCatalog();
    ASSERT_NE(second, first);
    first.reset();
    PropertyObjectPtr fresh = PropertyObject::Create(second, "class TestItem");
    ASSERT_EQ(fresh->Set("m_name", "New"), PropertySetResult::Ok);
    std::optional<TestItemView> const freshView = TestItemView::From(*fresh);
    ASSERT_TRUE(freshView);
    EXPECT_EQ(oldView->GetOrdinal(TestItemView::Name), 2u);
    EXPECT_EQ(freshView->GetOrdinal(TestItemView::Name), 3u);
    EXPECT_EQ(oldView->GetName(), "Old");
    EXPECT_EQ(freshView->GetName(), "New");
}
