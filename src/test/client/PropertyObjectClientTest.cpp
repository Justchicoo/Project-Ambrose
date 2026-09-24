/*
 * Project Ambrose by Imjustchico
 * Builds an object of every property class in the user's own r806919 type dump, when AMBROSE_TYPE_DUMP_PATH names it, and checks the load resolved every default the dump gives, every value passes its property's checks, each default form the dump writes lands on real properties, and each object clones equal.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "PropertyObject.h"

#include <gtest/gtest.h>

#include <string>

TEST(PropertyObjectClientTest, EveryPropertyClassBuildsWithValidDefaults)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump (format v2) from your own client to run this test";

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path)));
    TypeCatalogPtr const catalog = registry.GetCatalog();

    std::size_t built = 0;
    std::size_t defaulted = 0;
    std::size_t failures = 0;
    for (ClassInfo const* type : catalog->GetClasses())
    {
        if (type->Kind != ClassKind::PropertyClass)
            continue;
        PropertyObjectPtr const object = PropertyObject::Create(catalog, *type);
        if (!object)
        {
            if (++failures <= 20)
                ADD_FAILURE() << type->Name << " did not build";
            continue;
        }
        ++built;
        for (std::size_t ordinal = 0; ordinal < type->Properties.size(); ++ordinal)
        {
            PropertyInfo const& property = type->Properties[ordinal];
            if (PropertySetResult const result = PropertyObject::Check(property, *object->GetAt(ordinal)); result != PropertySetResult::Ok && ++failures <= 20)
                ADD_FAILURE() << type->Name << "::" << property.Name << ": " << PropertyObject::GetResultName(result);
            if (property.Default)
                ++defaulted;
        }
        if (!(*object->Clone() == *object) && ++failures <= 20)
            ADD_FAILURE() << type->Name << " does not clone equal";
    }
    EXPECT_EQ(failures, 0u);
    EXPECT_EQ(built, catalog->GetClassCount(ClassKind::PropertyClass));
    EXPECT_GT(defaulted, 0u);

    PropertyObjectPtr const pathAction = PropertyObject::Create(catalog, "class PathActionStop");
    PropertyObjectPtr const housing = PropertyObject::Create(catalog, "class HousingNode");
    PropertyObjectPtr const summon = PropertyObject::Create(catalog, "class SummonCinematicStageTemplate");
    PropertyObjectPtr const move = PropertyObject::Create(catalog, "class MoveBehaviorTemplate");
    PropertyObjectPtr const zone = PropertyObject::Create(catalog, "class WizZoneData");
    ASSERT_TRUE(pathAction && housing && summon && move && zone);
    EXPECT_EQ(*pathAction->Get("m_nNodeID")->GetIf<int32>(), -1);
    EXPECT_EQ(*housing->Get("m_locationType")->GetIf<uint32>(), 1u);
    EXPECT_EQ(*summon->Get("m_summonAnim")->GetIf<std::string>(), "Summon");
    EXPECT_EQ(*move->Get("m_fRunSpeed")->GetIf<float>(), 600.0f);
    EXPECT_EQ(*move->Get("m_fYawRate")->GetIf<float>(), 2.5f);
    EXPECT_EQ(*zone->Get("m_bloomScale")->GetIf<float>(), 0.970443f);
}
