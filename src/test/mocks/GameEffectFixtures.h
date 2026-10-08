/*
 * Project Ambrose by Imjustchico
 * The game effect classes laid out as the client's dump gives them, for tests that run without a client: GameEffectBase with its seven shared properties, NamedEffect deriving from it with its override name, GameEffectContainer with the public list the client receives and the private one it never does, BaseGameEffectBehavior holding the container, and the templates effects are made from: GameEffectTemplate with its name, category, duration, public and pet flags, NamedEffectTemplate making a NamedEffect, TransformationEffectTemplate making a TransformationEffect that shares its race and not its scale, which the effect holds as another type, StateEffectTemplate making no effect, and the GameEffectTemplateList a GameEffectData file holds; with a template made as the client's files hold one, a list file of templates, and a Root.wad of such files written where an install keeps it.
 */

#ifndef AMBROSE_GAMEEFFECTFIXTURES_H
#define AMBROSE_GAMEEFFECTFIXTURES_H

#include "BindFile.h"
#include "CharacterTypeFixtures.h"
#include "KiwadBuilder.h"
#include "PropertyObject.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace GameEffectFixtures
{
    inline constexpr uint32 Wire = 0x1F;
    inline constexpr uint32 Kept = 0x07;

    inline std::vector<std::pair<std::string, CharacterTypeFixtures::Detail::Json>> BaseProperties()
    {
        using CharacterTypeFixtures::Detail::Property;
        return {
            { "m_currentTickCount", Property("double", "m_currentTickCount", 0, Wire) },
            { "m_effectNameID", Property("unsigned int", "m_effectNameID", 1, Wire) },
            { "m_bIsOnPet", Property("bool", "m_bIsOnPet", 2, Wire) },
            { "m_originatorID", Property("gid", "m_originatorID", 3, Kept) },
            { "m_itemSlotID", Property("unsigned int", "m_itemSlotID", 4, Wire) },
            { "m_internalID", Property("int", "m_internalID", 5, Wire) },
            { "m_endTime", Property("unsigned int", "m_endTime", 6, Wire) } };
    }

    inline void AddClasses(CharacterTypeFixtures::Detail::Json& classes)
    {
        using CharacterTypeFixtures::Detail::AddClass;
        using CharacterTypeFixtures::Detail::Json;
        using CharacterTypeFixtures::Detail::Property;
        AddClass(classes, "class GameEffectBase", Json::array({ "PropertyClass" }), BaseProperties());
        std::vector<std::pair<std::string, Json>> named = BaseProperties();
        named.emplace_back("m_overrideName", Property("std::string", "m_overrideName", 7, Wire));
        AddClass(classes, "class NamedEffect", Json::array({ "GameEffectBase", "PropertyClass" }), std::move(named));
        AddClass(classes, "class GameEffectContainer", Json::array({ "PropertyClass" }), {
            { "m_publicEffects", Property("class SharedPointer<class GameEffectBase>", "m_publicEffects", 0, Wire, "List") },
            { "m_myEffects", Property("class SharedPointer<class GameEffectBase>", "m_myEffects", 1, Kept, "List") } });
        AddClass(classes, "class BaseGameEffectBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, 0x27) },
            { "m_gameEffects", Property("class SharedPointer<class GameEffectContainer>", "m_gameEffects", 1, Wire) } });
    }

    inline std::vector<std::pair<std::string, CharacterTypeFixtures::Detail::Json>> TemplateProperties()
    {
        using CharacterTypeFixtures::Detail::Property;
        return {
            { "m_effectName", Property("std::string", "m_effectName", 0, Wire) },
            { "m_effectCategory", Property("std::string", "m_effectCategory", 1, Wire) },
            { "m_duration", Property("double", "m_duration", 2, Wire) },
            { "m_isPublic", Property("bool", "m_isPublic", 3, Wire) },
            { "m_bIsOnPet", Property("bool", "m_bIsOnPet", 4, Wire) } };
    }

    inline void AddTemplateClasses(CharacterTypeFixtures::Detail::Json& classes)
    {
        using CharacterTypeFixtures::Detail::AddClass;
        using CharacterTypeFixtures::Detail::Json;
        using CharacterTypeFixtures::Detail::Property;
        AddClass(classes, "class GameEffectTemplate", Json::array({ "PropertyClass" }), TemplateProperties());
        AddClass(classes, "class NamedEffectTemplate", Json::array({ "GameEffectTemplate", "PropertyClass" }), TemplateProperties());
        AddClass(classes, "class StateEffectTemplate", Json::array({ "GameEffectTemplate", "PropertyClass" }), TemplateProperties());
        std::vector<std::pair<std::string, Json>> transformation = TemplateProperties();
        transformation.emplace_back("m_sRace", Property("std::string", "m_sRace", 5, Wire));
        transformation.emplace_back("m_fScale", Property("float", "m_fScale", 6, Wire));
        AddClass(classes, "class TransformationEffectTemplate", Json::array({ "GameEffectTemplate", "PropertyClass" }), std::move(transformation));
        std::vector<std::pair<std::string, Json>> transformed = BaseProperties();
        transformed.emplace_back("m_sRace", Property("std::string", "m_sRace", 7, Wire));
        transformed.emplace_back("m_fScale", Property("double", "m_fScale", 8, Wire));
        AddClass(classes, "class TransformationEffect", Json::array({ "GameEffectBase", "PropertyClass" }), std::move(transformed));
        AddClass(classes, "class GameEffectTemplateList", Json::array({ "PropertyClass" }), {
            { "m_effectTemplates", Property("class SharedPointer<class GameEffectTemplate>", "m_effectTemplates", 0, Wire, "List") } });
    }

    inline PropertyObjectPtr Template(TypeCatalogPtr const& catalog, std::string const& className, std::string const& name, double duration, bool isPublic, bool onPet)
    {
        PropertyObjectPtr made = PropertyObject::Create(catalog, className);
        EXPECT_TRUE(made) << className;
        if (!made)
            return nullptr;
        EXPECT_EQ(made->Set("m_effectName", name), PropertySetResult::Ok);
        EXPECT_EQ(made->Set("m_effectCategory", std::string("Test")), PropertySetResult::Ok);
        EXPECT_EQ(made->Set("m_duration", duration), PropertySetResult::Ok);
        EXPECT_EQ(made->Set("m_isPublic", isPublic), PropertySetResult::Ok);
        EXPECT_EQ(made->Set("m_bIsOnPet", onPet), PropertySetResult::Ok);
        return made;
    }

    inline std::vector<uint8> ListFile(TypeCatalogPtr const& catalog, std::vector<PropertyObjectPtr> templates)
    {
        PropertyObjectPtr list = PropertyObject::Create(catalog, "class GameEffectTemplateList");
        EXPECT_TRUE(list);
        if (!list)
            return {};
        PropertyValue::List listed;
        for (PropertyObjectPtr& made : templates)
            listed.emplace_back(std::move(made));
        EXPECT_EQ(list->Set("m_effectTemplates", std::move(listed)), PropertySetResult::Ok);
        EncodeResult encoded = BindFile::Write(list.get());
        EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
        return std::move(encoded.Bytes);
    }

    inline void WriteRoot(std::filesystem::path const& install, std::vector<std::pair<std::string, std::vector<uint8>>> const& files)
    {
        std::filesystem::path const folder = install / "Data" / "GameData";
        std::filesystem::create_directories(folder);
        KiwadBuilder builder(2);
        for (auto const& [name, bytes] : files)
            builder.Add(name, bytes, true);
        std::vector<uint8> const archive = builder.Build();
        std::ofstream stream(folder / "Root.wad", std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
    }
}

#endif
