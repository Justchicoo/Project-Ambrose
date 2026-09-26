/*
 * Project Ambrose by Imjustchico
 * Writes each class with its hash and properties the way the client's dump does, a SharedPointer or trailing asterisk marking a pointer, a derived class listing its bases' properties as its own, and each enum as an empty class its properties name with their options; writes objects as BINd and a Root.wad holding the manifest first and then each file.
 */

#include "TemplateDumpFixtures.h"
#include "BindFile.h"
#include "KiwadBuilder.h"
#include "StringHash.h"

#include <gtest/gtest.h>

#include <fstream>

namespace TemplateDumpFixtures
{
    namespace
    {
        constexpr uint32 Saved = 1 | 2 | 4;

        Json EffectProperties()
        {
            Json effect = Json::object();
            effect["m_effectType"] = Enum("enum SpellEffect::kSpellEffects", "m_effectType", 0, Json{ { "kInvalidSpellEffect", 0 }, { "kDamage", 1 }, { "kHeal", 3 } });
            effect["m_effectParam"] = Property("int", "m_effectParam", 1);
            effect["m_disposition"] = Enum("enum SpellEffect::kHangingDisposition", "m_disposition", 2, Json{ { "kBoth", 0 }, { "kBeneficial", 1 }, { "kHarmful", 2 } });
            effect["m_sDamageType"] = Property("std::string", "m_sDamageType", 3);
            effect["m_damageType"] = Property("unsigned int", "m_damageType", 4);
            effect["m_pipNum"] = Property("int", "m_pipNum", 5);
            effect["m_actNum"] = Property("int", "m_actNum", 6);
            effect["m_effectTarget"] = Enum("enum SpellEffect::kEffectTarget", "m_effectTarget", 7, Json{ { "kInvalidTarget", 0 }, { "kEnemySingle", 8 }, { "kSelf", 11 } });
            effect["m_numRounds"] = Property("int", "m_numRounds", 8);
            effect["m_paramPerRound"] = Property("int", "m_paramPerRound", 9);
            effect["m_healModifier"] = Property("float", "m_healModifier", 10);
            effect["m_spellTemplateID"] = Property("unsigned int", "m_spellTemplateID", 11);
            effect["m_enchantmentSpellTemplateID"] = Property("unsigned int", "m_enchantmentSpellTemplateID", 12);
            effect["m_cloaked"] = Property("bool", "m_cloaked", 13);
            effect["m_armorPiercingParam"] = Property("int", "m_armorPiercingParam", 14);
            effect["m_chancePerTarget"] = Property("int", "m_chancePerTarget", 15);
            effect["m_rank"] = Property("int", "m_rank", 16);
            return effect;
        }

        Json SpellProperties()
        {
            Json spell = Json::object();
            spell["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "List");
            spell["m_name"] = Property("std::string", "m_name", 1);
            spell["m_description"] = Property("std::string", "m_description", 2);
            spell["m_displayName"] = Property("std::string", "m_displayName", 3);
            spell["m_spellBase"] = Property("std::string", "m_spellBase", 4);
            spell["m_effects"] = Property("class SharedPointer<class SpellEffect>", "m_effects", 5, "List");
            spell["m_sMagicSchoolName"] = Property("std::string", "m_sMagicSchoolName", 6);
            spell["m_sTypeName"] = Property("std::string", "m_sTypeName", 7);
            spell["m_trainingCost"] = Property("int", "m_trainingCost", 8);
            spell["m_accuracy"] = Property("int", "m_accuracy", 9);
            spell["m_PvP"] = Property("bool", "m_PvP", 10);
            spell["m_PvE"] = Property("bool", "m_PvE", 11);
            spell["m_Treasure"] = Property("bool", "m_Treasure", 12);
            spell["m_spellSourceType"] = Enum("enum SpellTemplate::kSpellSourceType", "m_spellSourceType", 13, Json{ { "kCaster", 0 }, { "kPet", 1 } });
            spell["m_levelRestriction"] = Property("int", "m_levelRestriction", 14);
            spell["m_spellRank"] = Property("class SharedPointer<class SpellRank>", "m_spellRank", 15);
            spell["m_secondarySchoolName"] = Property("std::string", "m_secondarySchoolName", 16);
            spell["m_requiredSchoolName"] = Property("std::string", "m_requiredSchoolName", 17);
            return spell;
        }

        Json SigilProperties()
        {
            Json sigil = Json::object();
            sigil["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "List");
            sigil["m_sigilName"] = Property("std::string", "m_sigilName", 1);
            sigil["m_sigilType"] = Property("std::string", "m_sigilType", 2);
            sigil["m_useState"] = Property("bool", "m_useState", 3);
            sigil["m_subCircles"] = Property("class SigilSubCircle*", "m_subCircles", 4, "List");
            return sigil;
        }
    }

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container)
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    Json Enum(std::string const& type, std::string const& name, uint32 id, Json options)
    {
        Json property = Property(type, name, id);
        property["enum_options"] = std::move(options);
        return property;
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    void AddTemplateClasses(Json& classes)
    {
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        Json location = Json::object();
        location["m_filename"] = Property("std::string", "m_filename", 0);
        location["m_id"] = Property("unsigned int", "m_id", 1);
        AddClass(classes, "class TemplateLocation", Json::array({ "PropertyClass" }), location);
        Json manifest = Json::object();
        manifest["m_serializedTemplates"] = Property("class TemplateLocation", "m_serializedTemplates", 0, "List");
        AddClass(classes, "class TemplateManifest", Json::array({ "PropertyClass" }), manifest);
        Json behavior = Json::object();
        behavior["m_behaviorName"] = Property("std::string", "m_behaviorName", 0);
        AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), behavior);
        Json core = Json::object();
        core["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "List");
        AddClass(classes, "class CoreTemplate", Json::array({ "PropertyClass" }), core);
    }

    void AddSpellClasses(Json& classes)
    {
        for (char const* name : { "enum SpellEffect::kSpellEffects", "enum SpellEffect::kHangingDisposition", "enum SpellEffect::kEffectTarget", "enum SpellTemplate::kSpellSourceType" })
            AddClass(classes, name, Json::array(), Json::object());
        Json rank = Json::object();
        uint32 id = 0;
        for (char const* name : { "m_spellRank", "m_balancePips", "m_deathPips", "m_firePips", "m_icePips", "m_lifePips", "m_mythPips", "m_stormPips", "m_shadowPips" })
        {
            rank[name] = Property("unsigned char", name, id);
            ++id;
        }
        rank["m_xPipSpell"] = Property("bool", "m_xPipSpell", id);
        AddClass(classes, "class SpellRank", Json::array({ "PropertyClass" }), rank);
        AddClass(classes, "class SpellEffect", Json::array({ "PropertyClass" }), EffectProperties());
        Json random = EffectProperties();
        random["m_effectList"] = Property("class SharedPointer<class SpellEffect>", "m_effectList", 17, "List");
        AddClass(classes, "class RandomSpellEffect", Json::array({ "SpellEffect", "PropertyClass" }), random);
        Json element = Json::object();
        element["m_pEffect"] = Property("class SharedPointer<class SpellEffect>", "m_pEffect", 0);
        AddClass(classes, "class ConditionalSpellElement", Json::array({ "PropertyClass" }), element);
        Json conditional = EffectProperties();
        conditional["m_elements"] = Property("class SharedPointer<class ConditionalSpellElement>", "m_elements", 17, "List");
        AddClass(classes, "class ConditionalSpellEffect", Json::array({ "SpellEffect", "PropertyClass" }), conditional);
        AddClass(classes, "class SpellTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), SpellProperties());
        Json tiered = SpellProperties();
        tiered["m_retired"] = Property("bool", "m_retired", 18);
        AddClass(classes, "class TieredSpellTemplate", Json::array({ "SpellTemplate", "CoreTemplate", "PropertyClass" }), tiered);
        Json data = Json::object();
        data["m_tsGroupIndex"] = Property("int", "m_tsGroupIndex", 0);
        data["m_tsGroupTierOneSpellName"] = Property("std::string", "m_tsGroupTierOneSpellName", 1);
        AddClass(classes, "class TieredSpellGroupInfoData", Json::array({ "PropertyClass" }), data);
        Json info = Json::object();
        info["m_spellName"] = Property("std::string", "m_spellName", 0);
        info["m_theTieredSpellGroupInfoData"] = Property("class SharedPointer<class TieredSpellGroupInfoData>", "m_theTieredSpellGroupInfoData", 1);
        AddClass(classes, "class TieredSpellGroupInfo", Json::array({ "PropertyClass" }), info);
        Json list = Json::object();
        list["m_tieredSpellGroupInfoList"] = Property("class SharedPointer<class TieredSpellGroupInfo>", "m_tieredSpellGroupInfoList", 0, "List");
        AddClass(classes, "class TieredSpellGroupInfoList", Json::array({ "PropertyClass" }), list);
    }

    void AddSpellbookClasses(Json& classes)
    {
        Json tracker = Json::object();
        tracker["m_spellID"] = Property("unsigned int", "m_spellID", 0);
        tracker["m_isRetired"] = Property("bool", "m_isRetired", 1);
        tracker["m_tieredSpellGroupIndex"] = Property("int", "m_tieredSpellGroupIndex", 2);
        AddClass(classes, "class SpellIDTracker", Json::array({ "PropertyClass" }), tracker);
        Json behavior = Json::object();
        behavior["m_behaviorTemplateNameID"] = Property("unsigned int", "m_behaviorTemplateNameID", 0);
        behavior["m_spellIDList"] = Property("class SharedPointer<class SpellIDTracker>", "m_spellIDList", 1, "List");
        AddClass(classes, "class ClientSpellbookBehavior", Json::array({ "PropertyClass" }), behavior);
    }

    void AddSigilClasses(Json& classes)
    {
        AddClass(classes, "enum kShadow_Threshold_Type", Json::array(), Json::object());
        Json circle = Json::object();
        circle["m_locationType"] = Property("std::string", "m_locationType", 0);
        circle["m_locationPreference"] = Property("std::string", "m_locationPreference", 1);
        circle["m_rotation"] = Property("float", "m_rotation", 2);
        circle["m_radius"] = Property("float", "m_radius", 3);
        AddClass(classes, "class SigilSubCircle", Json::array({ "PropertyClass" }), circle);
        AddClass(classes, "class SigilTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), SigilProperties());
        AddClass(classes, "class MinigameSigilTemplate", Json::array({ "SigilTemplate", "CoreTemplate", "PropertyClass" }), SigilProperties());
        Json combat = SigilProperties();
        combat["m_engageRadius"] = Property("float", "m_engageRadius", 5);
        combat["m_battlefieldEffects"] = Property("class SharedPointer<class SpellEffect>", "m_battlefieldEffects", 6, "List");
        combat["m_shadowThresholdType"] = Enum("enum kShadow_Threshold_Type", "m_shadowThresholdType", 7, Json{ { "SHADOW_THRESHOLD_MIN", 0 }, { "SHADOW_THRESHOLD_MAX", 1 } });
        combat["m_shadowThresholdFactor"] = Property("float", "m_shadowThresholdFactor", 8);
        combat["m_shadowPipRatingFactor"] = Property("float", "m_shadowPipRatingFactor", 9);
        uint32 id = 10;
        for (char const* name : { "m_scalarDamagePvP", "m_scalarResistPvP", "m_scalarPiercePvP", "m_scalarDamagePvE", "m_scalarResistPvE", "m_scalarPiercePvE", "m_damageLimitPvP",
                 "m_dK0PvP", "m_dN0PvP", "m_resistLimitPvP", "m_rK0PvP", "m_rN0PvP", "m_damageLimitPvE", "m_dK0PvE", "m_dN0PvE", "m_resistLimitPvE", "m_rK0PvE", "m_rN0PvE" })
        {
            combat[name] = Property("float", name, id);
            ++id;
        }
        AddClass(classes, "class CombatSigilTemplate", Json::array({ "SigilTemplate", "CoreTemplate", "PropertyClass" }), combat);
    }

    std::string Dump(Json const& classes)
    {
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }

    std::vector<uint8> WriteBind(PropertyObjectPtr const& object)
    {
        EncodeResult encoded = BindFile::Write(object.get());
        EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
        return std::move(encoded.Bytes);
    }

    std::vector<uint8> Manifest(TypeCatalogPtr const& catalog, Locations const& locations)
    {
        PropertyObjectPtr manifest = PropertyObject::Create(catalog, "class TemplateManifest");
        EXPECT_TRUE(manifest);
        PropertyValue::List entries;
        for (auto const& [id, file] : locations)
        {
            PropertyObjectPtr location = PropertyObject::Create(catalog, "class TemplateLocation");
            EXPECT_EQ(location->Set("m_id", id), PropertySetResult::Ok);
            EXPECT_EQ(location->Set("m_filename", file), PropertySetResult::Ok);
            entries.emplace_back(std::move(location));
        }
        EXPECT_EQ(manifest->Set("m_serializedTemplates", std::move(entries)), PropertySetResult::Ok);
        return WriteBind(manifest);
    }

    void WriteRoot(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, Locations const& locations, Files const& files)
    {
        KiwadBuilder builder(2);
        builder.Add("TemplateManifest.xml", Manifest(catalog, locations), true);
        for (auto const& [path, bytes] : files)
            builder.Add(path, bytes, true);
        std::vector<uint8> const archive = builder.Build();
        std::filesystem::create_directories(gameData);
        std::ofstream(gameData / "Root.wad", std::ios::binary | std::ios::trunc).write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
    }
}
