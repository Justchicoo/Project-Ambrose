/*
 * Project Ambrose by Imjustchico
 * A format v2 type dump written by the project with the character select classes the login server builds, WizardCharacterCreationInfo, WizardCharacterBehavior and the equipment list, laid out with the property names, types, ids and flags the client's classes use, for login screen tests that run without a client.
 */

#ifndef AMBROSE_CHARACTERTYPEFIXTURES_H
#define AMBROSE_CHARACTERTYPEFIXTURES_H

#include "StringHash.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace CharacterTypeFixtures
{
    namespace Detail
    {
        using Json = nlohmann::json;

        inline Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags, std::string container = "Static")
        {
            bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
            return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
                { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
        }

        inline void AddClass(Json& classes, std::string const& name, Json bases, std::vector<std::pair<std::string, Json>> properties)
        {
            Json list = Json::object();
            for (auto& [propertyName, property] : properties)
                list[propertyName] = std::move(property);
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(list) } };
        }
    }

    inline std::string Dump()
    {
        using namespace Detail;
        constexpr uint32 Wire = 0x1F;
        constexpr uint32 Authority = 0x18;
        constexpr uint32 Local = 0x27;
        constexpr uint32 Appearance = 0x3F;
        constexpr uint32 EnumAppearance = 0x20003F;

        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), {});
        AddClass(classes, "enum eGender", Json::array(), {});
        AddClass(classes, "enum eRace", Json::array(), {});
        AddClass(classes, "class BehaviorInstance", Json::array({ "PropertyClass" }), { { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) } });

        std::vector<std::pair<std::string, Json>> const creation{
            { "m_templateID", Property("int", "m_templateID", 0, Wire) },
            { "m_name", Property("std::wstring", "m_name", 1, Wire) },
            { "m_shouldRename", Property("bool", "m_shouldRename", 2, Authority) },
            { "m_globalID", Property("gid", "m_globalID", 3, Wire) },
            { "m_userID", Property("gid", "m_userID", 4, Wire) },
            { "m_quarantined", Property("bool", "m_quarantined", 5, Authority) },
            { "m_lastLoginTime", Property("unsigned int", "m_lastLoginTime", 6, Authority) } };
        AddClass(classes, "class CharacterCreationInfo", Json::array({ "PropertyClass" }), creation);
        std::vector<std::pair<std::string, Json>> wizard = creation;
        wizard.emplace_back("m_avatarBehavior", Property("class SharedPointer<class WizardCharacterBehavior>", "m_avatarBehavior", 7, Wire));
        wizard.emplace_back("m_equipmentInfoList", Property("class SharedPointer<class EquippedItemInfoList>", "m_equipmentInfoList", 8, Wire));
        wizard.emplace_back("m_location", Property("std::string", "m_location", 9, Wire));
        wizard.emplace_back("m_level", Property("int", "m_level", 10, Wire));
        wizard.emplace_back("m_world", Property("int", "m_world", 11, Wire));
        wizard.emplace_back("m_schoolOfFocus", Property("unsigned int", "m_schoolOfFocus", 12, Wire));
        wizard.emplace_back("m_nameIndices", Property("unsigned int", "m_nameIndices", 13, Wire));
        AddClass(classes, "class WizardCharacterCreationInfo", Json::array({ "CharacterCreationInfo", "PropertyClass" }), wizard);

        Json gender = Property("enum eGender", "m_eGender", 17, EnumAppearance);
        gender["enum_options"] = Json{ { "Male", 1 }, { "Female", 0 }, { "Neutral", 2 } };
        AddClass(classes, "class WizardCharacterBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) },
            { "m_nHeadHandsModel", Property("bui2", "m_nHeadHandsModel", 1, Appearance) },
            { "m_nHairModel", Property("bui4", "m_nHairModel", 2, Appearance) },
            { "m_nHatModel", Property("bui2", "m_nHatModel", 3, Appearance) },
            { "m_nTorsoModel", Property("bui2", "m_nTorsoModel", 4, Appearance) },
            { "m_nFeetModel", Property("bui2", "m_nFeetModel", 5, Appearance) },
            { "m_nWandModel", Property("bui2", "m_nWandModel", 6, Appearance) },
            { "m_nSkinColor", Property("bui4", "m_nSkinColor", 7, Appearance) },
            { "m_nSkinDecal", Property("bui4", "m_nSkinDecal", 8, Appearance) },
            { "m_nHairColor", Property("bui7", "m_nHairColor", 9, Appearance) },
            { "m_nHatColor", Property("bui5", "m_nHatColor", 10, Appearance) },
            { "m_nHatDecal", Property("bui5", "m_nHatDecal", 11, Appearance) },
            { "m_nTorsoColor", Property("bui5", "m_nTorsoColor", 12, Appearance) },
            { "m_nTorsoDecal", Property("bui5", "m_nTorsoDecal", 13, Appearance) },
            { "m_nTorsoDecal2", Property("bui5", "m_nTorsoDecal2", 14, Appearance) },
            { "m_nFeetColor", Property("bui5", "m_nFeetColor", 15, Appearance) },
            { "m_nFeetDecal", Property("bui5", "m_nFeetDecal", 16, Appearance) },
            { "m_eGender", gender },
            { "m_eRace", Property("enum eRace", "m_eRace", 18, EnumAppearance) },
            { "m_afterCombatDance", Property("unsigned char", "m_afterCombatDance", 19, Appearance) },
            { "m_nSkinDecal2", Property("unsigned short", "m_nSkinDecal2", 20, Appearance) },
            { "m_extendedHairColor", Property("unsigned char", "m_extendedHairColor", 21, Appearance) },
            { "m_extendedSkinDecal", Property("unsigned short", "m_extendedSkinDecal", 22, Appearance) },
            { "m_newPlayerOptions", Property("unsigned int", "m_newPlayerOptions", 23, Appearance) },
            { "m_newPlayerOptions2", Property("unsigned int", "m_newPlayerOptions2", 24, Appearance) },
            { "m_afterCombatVictoryDance", Property("unsigned int", "m_afterCombatVictoryDance", 25, Wire) } });
        AddClass(classes, "class EquippedItemInfoList", Json::array({ "PropertyClass" }), { { "m_infoList", Property("class SharedPointer<class EquippedItemInfo>", "m_infoList", 0, Wire, "List") } });
        AddClass(classes, "class EquippedItemInfo", Json::array({ "PropertyClass" }), { { "m_itemID", Property("unsigned int", "m_itemID", 0, Wire) } });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }
}

#endif
