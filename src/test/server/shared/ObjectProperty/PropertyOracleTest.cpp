/*
 * Project Ambrose by Imjustchico
 * Tests the property oracle on a type dump the test writes: a hash made from a type and a name the dump lists together is named and marked known, one made from a type and a name that only appear apart is still named and marked as a guess, a name or type given by the caller extends what it can name, and a hash no pair makes gets no guess.
 */

#include "PropertyOracle.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    Json Property(std::string const& type, std::string const& name, uint32 id)
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", 7 }, { "container", "Static" }, { "dynamic", false }, { "singleton", false },
            { "pointer", false }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", Json::array({ "PropertyClass" }) }, { "hash", StringHash::KiStringHash(name) },
                { "properties", std::move(properties) } };
        };
        classes[std::to_string(StringHash::KiStringHash("class PropertyClass"))] = Json{ { "name", "class PropertyClass" }, { "bases", Json::array() },
            { "hash", StringHash::KiStringHash("class PropertyClass") }, { "properties", Json::object() } };
        add("class NpcSettings", Json{ { "m_npcProximity", Property("float", "m_npcProximity", 0) } });
        add("class QuestHolder", Json{ { "m_questList", Property("std::string", "m_questList", 0) } });
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "oracle.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }
}

TEST(PropertyOracleTest, APairTheDumpListsIsNamedAndMarkedKnown)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyOracle const oracle(*catalog);
    std::vector<PropertyGuess> const guesses = oracle.Guess(StringHash::PropertyHash("float", "m_npcProximity"));
    ASSERT_EQ(guesses.size(), 1u);
    EXPECT_EQ(guesses.front(), (PropertyGuess{ "float", "m_npcProximity", true }));
    EXPECT_EQ(oracle.GetNameCount(), 2u);
    EXPECT_EQ(oracle.GetTypeCount(), 2u);
}

TEST(PropertyOracleTest, ATypeAndANameThatOnlyAppearApartAreStillNamedAsAGuess)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyOracle const oracle(*catalog);
    std::vector<PropertyGuess> const guesses = oracle.Guess(StringHash::PropertyHash("float", "m_questList"));
    ASSERT_EQ(guesses.size(), 1u);
    EXPECT_EQ(guesses.front(), (PropertyGuess{ "float", "m_questList", false })) << "no class the dump lists holds a float m_questList";
}

TEST(PropertyOracleTest, NamesAndTypesTheCallerGivesExtendWhatItCanName)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    uint32 const hash = StringHash::PropertyHash("gid", "m_shardType");
    EXPECT_TRUE(PropertyOracle(*catalog).Guess(hash).empty()) << "neither the type nor the name is in the dump";
    PropertyOracle const extended(*catalog, { "m_shardType" }, { "gid" });
    std::vector<PropertyGuess> const guesses = extended.Guess(hash);
    ASSERT_EQ(guesses.size(), 1u);
    EXPECT_EQ(guesses.front(), (PropertyGuess{ "gid", "m_shardType", false }));
    EXPECT_TRUE(extended.Guess(0xDEADBEEFu).empty());
}
