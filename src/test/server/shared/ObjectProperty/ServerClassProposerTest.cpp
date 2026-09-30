/*
 * Project Ambrose by Imjustchico
 * Tests the server class proposer over a small dump written by the test: a behavior found in a template's behavior list is named by the one program string that hashes to it, derives from the behavior base the list holds with the base's property first, and not from a deeper class whose properties it happens to hold, keeps a property the dump lists elsewhere as the dump gives it and reads a new one with a trial container that a retry turns into a list; an enum property takes the options and the Enum flag the dump gives its type; a class only a text file of the install names says so in its evidence; a program string that does not hash to a class is no name for it; and a class nothing names, whose property the oracle cannot name, or that does not hold every property of the class its list holds is refused with the reason; and the element class of a list type, the list a path ends in and the class that list holds, walked from the path's root through each declared type, are read the way the proposer reads them. A class only the plain-XML files hold takes the server's own types: the one type, with its flags and options, the dump gives every property of an element's name, or of its last field for a name the dump lacks, when every value reads as it, else the first of bool, int, unsigned int, unsigned __int64, float, Color and std::string that does, an empty element reading as an empty value; objects are held by pointer to their class, or the nearest class they share, and a repeated or keyed element is a list; it derives from the class the dump property holding it declares; and it is refused with the reason when a BINd file holds it, its name is no class name, an element mixes text and objects or holds too many values, or its holders declare different classes.
 */

#include "PropertyFlags.h"
#include "PropertyOracle.h"
#include "ServerClassProposer.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", uint32 flags = 7, bool pointer = false)
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    constexpr uint32 EnumBit = uint32{ 1 } << 21;

    uint32 Hash(std::string const& type, std::string const& name)
    {
        return StringHash::PropertyHash(type, name);
    }

    class ServerClassProposerTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Json classes = Json::object();
            AddClass(classes, "class PropertyClass", Json::array(), Json::object());
            AddClass(classes, "enum Shade", Json::array(), Json::object());
            Json behavior = Json::object();
            behavior["m_behaviorName"] = Property("std::string", "m_behaviorName", 0);
            AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), behavior);
            Json named = behavior;
            named["m_displayName"] = Property("std::string", "m_displayName", 1);
            AddClass(classes, "class NamedBehaviorTemplate", Json::array({ "BehaviorTemplate", "PropertyClass" }), named);
            Json object = Json::object();
            object["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "Vector", 7, true);
            object["m_tags"] = Property("std::string", "m_tags", 1, "List", 7);
            Json colored = Property("enum Shade", "m_shade", 2, "Static", 7 | EnumBit);
            colored["enum_options"] = Json{ { "Shade_Light", 0 }, { "Shade_Dark", 1 } };
            object["m_shade"] = colored;
            AddClass(classes, "class GameObjectTemplate", Json::array({ "PropertyClass" }), object);
            AddClass(classes, "class Color", Json::array(), Json::object());
            Json samples = Json::object();
            samples["m_sampleFlag"] = Property("bool", "m_sampleFlag", 0);
            samples["m_sampleCount"] = Property("int", "m_sampleCount", 1);
            samples["m_sampleSize"] = Property("unsigned int", "m_sampleSize", 2);
            samples["m_sampleBig"] = Property("unsigned __int64", "m_sampleBig", 3);
            samples["m_sampleScale"] = Property("float", "m_sampleScale", 4);
            samples["m_sampleTint"] = Property("class Color", "m_sampleTint", 5);
            samples["m_visible"] = Property("bool", "m_visible", 6);
            samples["m_id.m_full"] = Property("unsigned __int64", "m_id.m_full", 7);
            samples["m_level"] = Property("int", "m_level", 8);
            AddClass(classes, "class ValueSamples", Json::array({ "PropertyClass" }), samples);
            Json more = Json::object();
            more["m_visible"] = Property("bool", "m_visible", 0);
            more["m_level"] = Property("unsigned int", "m_level", 1);
            AddClass(classes, "class MoreSamples", Json::array({ "PropertyClass" }), more);
            Json shelf = Json::object();
            shelf["m_things"] = Property("class GameObjectTemplate*", "m_things", 0, "List", 7, true);
            AddClass(classes, "class Shelf", Json::array({ "PropertyClass" }), shelf);
            Json const dump{ { "version", 2 }, { "classes", classes } };
            ASSERT_TRUE(_registry.LoadFromText(dump.dump(), "proposer.json")) << (_registry.GetErrors().empty() ? std::string() : _registry.GetErrors().front());
            _catalog = _registry.GetCatalog();
        }

        ServerClassObservation Seen(std::string const& name, std::vector<uint32> properties, std::string path = "class GameObjectTemplate.m_behaviors[3]")
        {
            return ServerClassObservation{ StringHash::KiStringHash(name), 12, 4, "ObjectData/Thing.xml", std::move(path), std::move(properties) };
        }

        static XmlSweepProperty Element(std::string name, std::set<std::string> values, uint64 empty = 0, bool repeats = false, bool keyed = false, std::set<std::string> held = {})
        {
            XmlSweepProperty property;
            property.Name = std::move(name);
            property.Count = values.size() + held.size() + empty;
            property.Empty = empty;
            property.Repeats = repeats;
            property.Keyed = keyed;
            property.Held = std::move(held);
            property.Values = std::move(values);
            return property;
        }

        static XmlSweepClass XmlSeen(std::string name, std::vector<XmlSweepProperty> properties, std::set<std::pair<std::string, std::string>> holders = {})
        {
            XmlSweepClass seen;
            seen.Name = std::move(name);
            seen.Count = 3;
            seen.Files = 2;
            seen.FirstFile = "Chatter.xml";
            seen.FirstPath = seen.Name;
            seen.AtRoot = holders.empty();
            seen.Holders = std::move(holders);
            seen.Properties = std::move(properties);
            return seen;
        }

        static TypeDumpLoader::RawProperty const* Find(TypeDumpLoader::RawClass const& type, std::string const& name)
        {
            for (TypeDumpLoader::RawProperty const& property : type.Properties)
                if (property.Name == name)
                    return &property;
            return nullptr;
        }

        static std::string Reason(ServerClassProposals const& proposals, std::string const& name)
        {
            for (ServerClassRefusal const& refusal : proposals.Refused)
                if (refusal.Hash == StringHash::KiStringHash(name))
                    return refusal.Reason;
            return {};
        }

        TypeRegistry _registry;
        TypeCatalogPtr _catalog;
    };
}

TEST_F(ServerClassProposerTest, ABehaviorDerivesFromTheClassItsListHoldsAndReadsANewPropertyWithATrialContainer)
{
    PropertyOracle const oracle(*_catalog, { "m_spellList", "m_deckColor" }, { "unsigned int" });
    ServerClassNames const names{ { StringHash::KiStringHash("class DeckBehaviorTemplate"), { { "class DeckBehaviorTemplate" } } } };
    std::vector<ServerClassObservation> const seen{ Seen("class DeckBehaviorTemplate",
        { Hash("std::string", "m_behaviorName"), Hash("std::string", "m_tags"), Hash("unsigned int", "m_spellList") }) };

    ServerClassProposals const first = ServerClassProposer::Propose(*_catalog, oracle, names, seen);
    ASSERT_TRUE(first.Refused.empty()) << first.Refused.front().Reason;
    ASSERT_EQ(first.Classes.size(), 1u);
    TypeDumpLoader::RawClass const& deck = first.Classes.front().Class;
    EXPECT_EQ(*deck.Name, "class DeckBehaviorTemplate");
    EXPECT_EQ(deck.Bases, (std::vector<std::string>{ "class BehaviorTemplate", "class PropertyClass" })) << "the class the list holds is the base the data proves";
    ASSERT_EQ(deck.Properties.size(), 3u);
    EXPECT_EQ(deck.Properties[0].Name, "m_behaviorName") << "the base's properties come first";
    EXPECT_EQ(*deck.Properties[0].Offset, 8u);
    TypeDumpLoader::RawProperty const& tags = deck.Properties[1].Name == "m_tags" ? deck.Properties[1] : deck.Properties[2];
    TypeDumpLoader::RawProperty const& spells = deck.Properties[1].Name == "m_spellList" ? deck.Properties[1] : deck.Properties[2];
    EXPECT_EQ(*tags.Container, "List") << "a type and name the dump lists elsewhere keeps the dump's container";
    EXPECT_EQ(*tags.Flags, 7u);
    EXPECT_EQ(*spells.Container, "Static");
    EXPECT_EQ(*spells.Flags, PropertyFlags::Bit(PropertyFlag::Save)) << "a new property is known only to be saved";
    EXPECT_FALSE(*spells.Dynamic);
    EXPECT_EQ(first.Classes.front().Trials, (std::set<uint32>{ Hash("unsigned int", "m_spellList") }));
    for (std::size_t index = 0; index < deck.Properties.size(); ++index)
        EXPECT_EQ(*deck.Properties[index].Id, index);
    EXPECT_NE(first.Classes.front().Evidence.find("12 object(s) in 4 file(s)"), std::string::npos) << first.Classes.front().Evidence;
    EXPECT_NE(first.Classes.front().Evidence.find("The client program holds the string class DeckBehaviorTemplate"), std::string::npos) << first.Classes.front().Evidence;

    ServerClassContainers const retry{ { { StringHash::KiStringHash("class DeckBehaviorTemplate"), Hash("unsigned int", "m_spellList") }, "List" } };
    ServerClassProposals const second = ServerClassProposer::Propose(*_catalog, oracle, names, seen, retry);
    ASSERT_EQ(second.Classes.size(), 1u);
    for (TypeDumpLoader::RawProperty const& property : second.Classes.front().Class.Properties)
        if (property.Name == "m_spellList")
        {
            EXPECT_EQ(*property.Container, "List");
            EXPECT_TRUE(*property.Dynamic);
        }
}

TEST_F(ServerClassProposerTest, AClassOnlyATextFileNamesSaysSoInItsEvidence)
{
    PropertyOracle const oracle(*_catalog);
    ServerClassNames const names{ { StringHash::KiStringHash("class ChestBehaviorTemplate"), { { "class ChestBehaviorTemplate", true } } } };
    ServerClassProposals const proposed = ServerClassProposer::Propose(*_catalog, oracle, names,
        { Seen("class ChestBehaviorTemplate", { Hash("std::string", "m_behaviorName"), Hash("std::string", "m_displayName") }) });
    ASSERT_EQ(proposed.Classes.size(), 1u);
    EXPECT_NE(proposed.Classes.front().Evidence.find("A text file of the install's archives writes the class class ChestBehaviorTemplate, which hashes to it"), std::string::npos)
        << proposed.Classes.front().Evidence;
}

TEST_F(ServerClassProposerTest, AClassThatHoldsExactlyADeeperClasssPropertiesIsNotTakenToDeriveFromIt)
{
    PropertyOracle const oracle(*_catalog);
    ServerClassNames const names{ { StringHash::KiStringHash("class ChestBehaviorTemplate"), { { "class ChestBehaviorTemplate" } } } };
    ServerClassProposals const proposed = ServerClassProposer::Propose(*_catalog, oracle, names,
        { Seen("class ChestBehaviorTemplate", { Hash("std::string", "m_behaviorName"), Hash("std::string", "m_displayName") }) });
    ASSERT_EQ(proposed.Classes.size(), 1u);
    EXPECT_EQ(proposed.Classes.front().Class.Bases.front(), "class BehaviorTemplate") << "it holds NamedBehaviorTemplate's properties, but may declare m_displayName itself";
    EXPECT_EQ(proposed.Classes.front().Class.Properties.size(), 2u);
}

TEST_F(ServerClassProposerTest, ANewEnumPropertyTakesTheOptionsTheDumpGivesItsType)
{
    PropertyOracle const oracle(*_catalog, { "m_trimShade" }, {});
    ServerClassNames const names{ { StringHash::KiStringHash("class ShadedBehaviorTemplate"), { { "class ShadedBehaviorTemplate" } } } };
    ServerClassProposals const proposed = ServerClassProposer::Propose(*_catalog, oracle, names,
        { Seen("class ShadedBehaviorTemplate", { Hash("std::string", "m_behaviorName"), Hash("enum Shade", "m_trimShade") }) });
    ASSERT_EQ(proposed.Classes.size(), 1u);
    TypeDumpLoader::RawProperty const& shade = proposed.Classes.front().Class.Properties.back();
    EXPECT_EQ(shade.Name, "m_trimShade");
    EXPECT_EQ(*shade.Flags, PropertyFlags::Bit(PropertyFlag::Save) | EnumBit) << "the dump reads its type's values as an enum";
    ASSERT_EQ(shade.Options.size(), 2u);
    EXPECT_TRUE(std::any_of(shade.Options.begin(), shade.Options.end(), [](auto const& option) { return option.first == "Shade_Light"; }));
}

TEST_F(ServerClassProposerTest, AClassThatCannotBeNamedTypedOrBasedIsRefusedWithTheReason)
{
    PropertyOracle const oracle(*_catalog, { "m_spellList" }, { "unsigned int" });
    uint32 const known = Hash("std::string", "m_behaviorName");
    ServerClassNames names{ { StringHash::KiStringHash("class TwiceNamed"), { { "class TwiceNamed" }, { "class Imposter" } } },
        { StringHash::KiStringHash("class OddTemplate"), { { "class OddTemplate" } } }, { StringHash::KiStringHash("class LooseTemplate"), { { "class LooseTemplate" } } } };
    std::vector<ServerClassObservation> const seen{ Seen("class Nameless", { known }), Seen("class TwiceNamed", { known }), Seen("class OddTemplate", { known, 424242 }),
        Seen("class LooseTemplate", { Hash("unsigned int", "m_spellList") }) };
    ServerClassProposals const proposed = ServerClassProposer::Propose(*_catalog, oracle, names, seen);
    ASSERT_EQ(proposed.Classes.size(), 1u);
    EXPECT_EQ(*proposed.Classes.front().Class.Name, "class TwiceNamed") << "a program string that does not hash to the class is not a name for it";
    ASSERT_EQ(proposed.Refused.size(), 3u);
    auto const reason = [&proposed](std::string const& name) {
        for (ServerClassRefusal const& refusal : proposed.Refused)
            if (refusal.Hash == StringHash::KiStringHash(name))
                return refusal.Reason;
        return std::string();
    };
    EXPECT_EQ(reason("class Nameless"), "no name the client program's strings or the install's text files give hashes to it");
    EXPECT_NE(reason("class OddTemplate").find("its property 424242 is named no way"), std::string::npos) << reason("class OddTemplate");
    EXPECT_NE(reason("class LooseTemplate").find("does not hold every property of class BehaviorTemplate"), std::string::npos) << reason("class LooseTemplate");
}

TEST_F(ServerClassProposerTest, TheElementClassOfAListTypeAndTheListAPathEndsInAreReadAsTheProposerReadsThem)
{
    EXPECT_EQ(ServerClassProposer::ElementClass("class BehaviorTemplate*"), "class BehaviorTemplate");
    EXPECT_EQ(ServerClassProposer::ElementClass("class SharedPointer<class Result>"), "class Result");
    EXPECT_EQ(ServerClassProposer::ElementClass("struct Point"), "struct Point");
    EXPECT_FALSE(ServerClassProposer::ElementClass("std::string"));
    EXPECT_FALSE(ServerClassProposer::ElementClass("enum Shade"));
    EXPECT_EQ(ServerClassProposer::LastProperty("class GameObjectTemplate.m_behaviors[0].m_activatedResultList.m_results[2]"), "m_results");
    EXPECT_EQ(ServerClassProposer::LastProperty("class WizGameObjectTemplate.m_behaviors[10]"), "m_behaviors");
    EXPECT_EQ(ServerClassProposer::LastProperty("class Root"), "");
}

TEST_F(ServerClassProposerTest, TheListAPathEndsInIsReadThroughEachPropertysDeclaredType)
{
    std::string problem;
    ClassInfo const* const held = ServerClassProposer::HeldClass(*_catalog, "class GameObjectTemplate.m_behaviors[3]", problem);
    ASSERT_NE(held, nullptr) << problem;
    EXPECT_EQ(held->Name, "class BehaviorTemplate");
    EXPECT_EQ(ServerClassProposer::HeldClass(*_catalog, "the root object", problem), nullptr);
    EXPECT_TRUE(problem.empty()) << "a root object sits in no list, which is no problem";
    EXPECT_EQ(ServerClassProposer::HeldClass(*_catalog, "class GameObjectTemplate.m_behaviors[0].m_nothing", problem), nullptr);
    EXPECT_NE(problem.find("no class of the dump derived from class BehaviorTemplate declares m_nothing"), std::string::npos) << problem;
    problem.clear();
    EXPECT_EQ(ServerClassProposer::HeldClass(*_catalog, "class Missing.m_behaviors[0]", problem), nullptr);
    EXPECT_NE(problem.find("class Missing"), std::string::npos) << problem;
}

TEST_F(ServerClassProposerTest, AClassOnlyTheXmlFilesHoldTakesTheServersOwnTypesFromItsValuesAndNames)
{
    std::vector<XmlSweepClass> const seen{ XmlSeen("class ChatterManager", {
        Element("m_id.m_full", { "84099", "84100" }),
        Element("m_owner.m_full", { "42" }),
        Element("m_owner.m_part", { "7" }),
        Element("m_visible", { "1" }),
        Element("m_shade", { "Shade_Dark" }),
        Element("m_level", { "3" }),
        Element("m_count", { "3", "-2" }),
        Element("m_size", { "3000000000" }),
        Element("m_big", { "5000000000" }),
        Element("m_weight", { "1.5", "2" }),
        Element("m_colors", { "FF0808AC", "FF0061CE" }, 0, true),
        Element("m_enabled", { "true", "False" }),
        Element("m_lines", { "Chatter_1" }, 0, false, true),
        Element("m_note", {}, 2),
        Element("m_items", {}, 1, true, true, { "class Chatter" }) }) };
    ServerClassProposals const proposed = ServerClassProposer::ProposeXml(*_catalog, seen);
    ASSERT_TRUE(proposed.Refused.empty()) << proposed.Refused.front().Reason;
    ASSERT_EQ(proposed.Classes.size(), 1u);
    TypeDumpLoader::RawClass const& manager = proposed.Classes.front().Class;
    EXPECT_EQ(*manager.Name, "class ChatterManager");
    EXPECT_EQ(*manager.Hash, StringHash::KiStringHash("class ChatterManager"));
    EXPECT_EQ(manager.Bases, (std::vector<std::string>{ "class PropertyClass" })) << "nothing in the dump holds it";
    ASSERT_EQ(manager.Properties.size(), 15u);

    auto const expect = [&manager](std::string const& name, std::string const& type, std::string const& container)
    {
        TypeDumpLoader::RawProperty const* const property = Find(manager, name);
        ASSERT_NE(property, nullptr) << name;
        EXPECT_EQ(*property->Type, type) << name;
        EXPECT_EQ(*property->Container, container) << name;
        EXPECT_EQ(*property->Hash, StringHash::PropertyHash(type, name)) << name;
        EXPECT_EQ(*property->Dynamic, container != "Static") << name;
    };
    expect("m_id.m_full", "unsigned __int64", "Static");
    expect("m_owner.m_full", "unsigned __int64", "Static");
    expect("m_owner.m_part", "int", "Static");
    expect("m_visible", "bool", "Static");
    expect("m_shade", "enum Shade", "Static");
    expect("m_level", "int", "Static");
    expect("m_count", "int", "Static");
    expect("m_size", "unsigned int", "Static");
    expect("m_big", "unsigned __int64", "Static");
    expect("m_weight", "float", "Static");
    expect("m_colors", "class Color", "List");
    expect("m_enabled", "bool", "Static");
    expect("m_lines", "std::string", "List");
    expect("m_note", "std::string", "Static");
    expect("m_items", "class Chatter*", "List");
    EXPECT_TRUE(*Find(manager, "m_items")->Pointer) << "an object is held by pointer, which reads a null as well";
    EXPECT_FALSE(*Find(manager, "m_note")->Pointer);
    EXPECT_EQ(*Find(manager, "m_shade")->Flags, PropertyFlags::Bit(PropertyFlag::Save) | EnumBit) << "the name's type comes with its value flags and options";
    EXPECT_EQ(Find(manager, "m_shade")->Options.size(), 2u);
    EXPECT_EQ(*Find(manager, "m_level")->Flags, PropertyFlags::Bit(PropertyFlag::Save));
    for (std::size_t index = 0; index < manager.Properties.size(); ++index)
        EXPECT_EQ(*manager.Properties[index].Id, index);
    std::string const& evidence = proposed.Classes.front().Evidence;
    EXPECT_NE(evidence.find("3 object(s) in 2 plain-XML file(s)"), std::string::npos) << evidence;
    EXPECT_NE(evidence.find("Its 15 own properties"), std::string::npos) << evidence;
    EXPECT_NE(evidence.find("1 hold objects"), std::string::npos) << evidence;
    EXPECT_NE(evidence.find("4 take the one type the dump gives every property of that name, or of that field of a wrapped value"), std::string::npos) << evidence;
    EXPECT_NE(evidence.find("and 10 the first of bool"), std::string::npos) << evidence;
    EXPECT_NE(evidence.find("3 are lists"), std::string::npos) << evidence;
}

TEST_F(ServerClassProposerTest, AnXmlClassDerivesFromTheClassTheDumpPropertyHoldingItDeclares)
{
    std::vector<XmlSweepClass> const seen{
        XmlSeen("class ExtraBehaviorTemplate", { Element("m_behaviorName", { "Extra" }), Element("m_extra", { "7" }) }, { { "class GameObjectTemplate", "m_behaviors" } }),
        XmlSeen("class Rack", { Element("m_held", {}, 0, true, false, { "class BehaviorTemplate", "class NamedBehaviorTemplate" }),
                                  Element("m_mixed", {}, 0, true, false, { "class NamedBehaviorTemplate", "class Chatter" }) }) };
    ServerClassProposals const proposed = ServerClassProposer::ProposeXml(*_catalog, seen);
    ASSERT_TRUE(proposed.Refused.empty()) << proposed.Refused.front().Reason;
    ASSERT_EQ(proposed.Classes.size(), 2u);
    for (ServerClassProposal const& proposal : proposed.Classes)
    {
        TypeDumpLoader::RawClass const& type = proposal.Class;
        if (*type.Name == "class ExtraBehaviorTemplate")
        {
            EXPECT_EQ(type.Bases, (std::vector<std::string>{ "class BehaviorTemplate", "class PropertyClass" }));
            ASSERT_EQ(type.Properties.size(), 2u) << "the base's element is the base's property, not a second one";
            EXPECT_EQ(type.Properties[0].Name, "m_behaviorName");
            EXPECT_EQ(*type.Properties[0].Offset, 8u) << "copied as the dump gives it";
            EXPECT_EQ(type.Properties[1].Name, "m_extra");
            EXPECT_NE(proposal.Evidence.find("the class the dump property holding it declares"), std::string::npos) << proposal.Evidence;
        }
        else
        {
            EXPECT_EQ(*Find(type, "m_held")->Type, "class BehaviorTemplate*") << "the nearest class every object it holds derives from";
            EXPECT_EQ(*Find(type, "m_mixed")->Type, "class PropertyClass*") << "a class the dump does not describe derives from nothing it knows";
        }
    }
}

TEST_F(ServerClassProposerTest, AnXmlClassIsRefusedWithTheReasonWhenItsValuesOrPlacesCannotGiveItOneShape)
{
    XmlSweepProperty mixed = Element("m_both", {}, 0, false, false, { "class Chatter" });
    mixed.Mixed = true;
    XmlSweepProperty crowded = Element("m_many", { "a" });
    crowded.Overflow = true;
    std::vector<XmlSweepClass> const seen{
        XmlSeen("class Bound", { Element("m_value", { "1" }) }),
        XmlSeen("Loose", { Element("m_value", { "1" }) }),
        XmlSeen("class Muddled", { mixed }),
        XmlSeen("class Torn", { Element("m_either", { "text" }, 0, false, false, { "class Chatter" }) }),
        XmlSeen("class Crowded", { crowded }),
        XmlSeen("class Split", { Element("m_value", { "1" }) }, { { "class GameObjectTemplate", "m_behaviors" }, { "class Shelf", "m_things" } }),
        XmlSeen("class BehaviorTemplate", { Element("m_behaviorName", { "Known" }) }) };
    ServerClassProposals const proposed = ServerClassProposer::ProposeXml(*_catalog, seen, { StringHash::KiStringHash("class Bound") });
    EXPECT_TRUE(proposed.Classes.empty());
    EXPECT_EQ(proposed.Refused.size(), 6u) << "a class the dump describes is not proposed nor refused";
    EXPECT_EQ(Reason(proposed, "class Bound"), "BINd files hold it too, whose property hashes prove its types, so it is proposed from them");
    EXPECT_EQ(Reason(proposed, "Loose"), "the files name it 'Loose', which is not a class name");
    EXPECT_NE(Reason(proposed, "class Muddled").find("its element <m_both> holds text beside an object"), std::string::npos) << Reason(proposed, "class Muddled");
    EXPECT_NE(Reason(proposed, "class Torn").find("its element <m_either>"), std::string::npos) << Reason(proposed, "class Torn");
    EXPECT_NE(Reason(proposed, "class Crowded").find("more than 65536 distinct values"), std::string::npos) << Reason(proposed, "class Crowded");
    EXPECT_NE(Reason(proposed, "class Split").find("declare 2 different classes"), std::string::npos) << Reason(proposed, "class Split");
}
