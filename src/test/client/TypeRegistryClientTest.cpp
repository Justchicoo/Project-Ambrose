/*
 * Project Ambrose by Imjustchico
 * Loads the user's own r806919 type dump into the type registry when AMBROSE_TYPE_DUMP_PATH names it, and checks the class kind counts for the shape the dump's own header names, the reference dump's or the larger one this project's extractor writes, that every property type is classified, the load time and size, WizClientObject's first properties in id order, that pointer aliases of unprefixed templates join their class, and that integer defaults are not taken for enum options.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "TypeDumpCache.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
    constexpr std::string_view PinnedRevision = "r806919.Wizard_1_610";
    constexpr std::string_view OurExtractor = "typeextract";
}

TEST(TypeRegistryClientTest, TheR806919DumpLoadsAndClassifiesEveryType)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump (format v2) from your own client to run this test";

    std::optional<TypeDumpHeader> const header = TypeDumpCache::ReadHeader(LogConfig::Utf8Path(*path));
    if (header && !header->Revision.empty() && header->Revision != PinnedRevision)
        GTEST_SKIP() << "these counts are r806919.Wizard_1_610's, and AMBROSE_TYPE_DUMP_PATH names " << header->Revision;
    bool const ours = header && header->Extractor.starts_with(OurExtractor);

    TypeRegistry registry;
    auto const start = std::chrono::steady_clock::now();
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path))) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    auto const elapsed = std::chrono::steady_clock::now() - start;
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_TRUE(catalog);

    EXPECT_EQ(catalog->GetClassCount(ClassKind::PropertyClass), ours ? 2198u : 2197u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Enum), ours ? 141u : 140u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Primitive), 37u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Container), 168u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::ValueType), 13u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Opaque), ours ? 39u : 37u);
    EXPECT_EQ(catalog->GetClasses().size(), ours ? 2596u : 2592u);
    EXPECT_EQ(catalog->GetAliasCount(), ours ? 4398u : 4397u);
    EXPECT_EQ(catalog->GetPropertyCount(), ours ? 16495u : 16493u);
    EXPECT_LT(elapsed, std::chrono::seconds(10));
    EXPECT_LT(catalog->GetApproximateBytes(), std::size_t{ 150 } << 20);
    std::cout << "Loaded the type dump in " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms, about " << ((catalog->GetApproximateBytes() + (std::size_t{ 1 } << 19)) >> 20) << " MiB\n";

    ClassInfo const* const wizClientObject = catalog->FindClass("class WizClientObject");
    ASSERT_NE(wizClientObject, nullptr);
    ASSERT_EQ(wizClientObject->Properties.size(), 14u);
    EXPECT_EQ(wizClientObject->Properties[0].Name, "m_inactiveBehaviors");
    EXPECT_EQ(wizClientObject->Properties[1].Name, "m_globalID.m_full");
    EXPECT_EQ(wizClientObject->Properties[2].Name, "m_permID");
    EXPECT_EQ(wizClientObject->Properties[0].Kind, ValueKind::Object);
    EXPECT_EQ(wizClientObject->Properties[0].Container, ContainerKind::Vector);
    ClassInfo const* const clientObject = catalog->FindClass("class ClientObject");
    ASSERT_NE(clientObject, nullptr);
    EXPECT_TRUE(wizClientObject->IsA(*clientObject));
    EXPECT_EQ(catalog->FindClass("class SharedPointer<class WizClientObject>"), wizClientObject);

    ClassInfo const* const madlibFloat = catalog->FindClass("MadlibArgT<float>");
    ASSERT_NE(madlibFloat, nullptr);
    EXPECT_EQ(catalog->FindClass("class MadlibArgT<float>*"), madlibFloat);
    EXPECT_EQ(catalog->FindClass("class MadlibArgT<float>"), nullptr);

    std::size_t integerDefaults = 0;
    for (ClassInfo const* info : catalog->GetClasses())
        for (PropertyInfo const& property : info->Properties)
        {
            EXPECT_FALSE(property.FindOptionValue("__DEFAULT")) << info->Name << "::" << property.Name;
            if (property.Default && std::holds_alternative<int64>(*property.Default))
                ++integerDefaults;
        }
    EXPECT_GT(integerDefaults, 0u);
}
