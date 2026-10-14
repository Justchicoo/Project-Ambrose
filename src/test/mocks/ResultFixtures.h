/*
 * Project Ambrose by Imjustchico
 * A type dump and catalog holding the result classes tests run, ResClientNotifyText, ResAddDynaMod, ResRemoveDynaMod, ResTeleport, ResSpawn, the quest rewards ResAddGold and ResAddMagicXP and two classes only tests use, with decoded results of them built ready to run and encoded as zone_trigger_result holds them.
 */

#ifndef AMBROSE_RESULTFIXTURES_H
#define AMBROSE_RESULTFIXTURES_H

#include "CharacterTypeFixtures.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "ResultMgr.h"
#include "TypeRegistry.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ResultFixtures
{
    constexpr std::string_view FirstTestClass = "class ResultFixtureFirst";
    constexpr std::string_view SecondTestClass = "class ResultFixtureSecond";

    inline std::string Dump()
    {
        using CharacterTypeFixtures::Detail::AddClass;
        using CharacterTypeFixtures::Detail::Json;
        using CharacterTypeFixtures::Detail::Property;
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), {});
        AddClass(classes, "class ResClientNotifyText", Json::array({ "PropertyClass" }), { { "m_text", Property("std::string", "m_text", 0, 7) }, { "m_type", Property("int", "m_type", 1, 7) } });
        AddClass(classes, "class ResAddDynaMod", Json::array({ "PropertyClass" }), { { "m_dynaModClientTag", Property("std::string", "m_dynaModClientTag", 0, 7) },
            { "m_dynaModRemove", Property("bool", "m_dynaModRemove", 1, 7) }, { "m_dynaModState", Property("std::string", "m_dynaModState", 2, 7) },
            { "m_zoneName", Property("std::string", "m_zoneName", 3, 7) } });
        AddClass(classes, "class ResRemoveDynaMod", Json::array({ "PropertyClass" }), { { "m_dynaModClientTag", Property("std::string", "m_dynaModClientTag", 0, 7) },
            { "m_useQuestAsOriginator", Property("bool", "m_useQuestAsOriginator", 1, 7) } });
        AddClass(classes, "class ResTeleport", Json::array({ "PropertyClass" }), {});
        AddClass(classes, "class ResSpawn", Json::array({ "PropertyClass" }), {});
        AddClass(classes, "class ResAddGold", Json::array({ "PropertyClass" }), { { "m_gold", Property("int", "m_gold", 0, 1) }, { "m_sourceType", Property("std::string", "m_sourceType", 1, 1) } });
        AddClass(classes, "class ResAddMagicXP", Json::array({ "PropertyClass" }), { { "m_experience", Property("int", "m_experience", 0, 1) },
            { "m_sourceType", Property("std::string", "m_sourceType", 1, 1) }, { "m_magicSchool", Property("std::string", "m_magicSchool", 2, 1) } });
        AddClass(classes, std::string(FirstTestClass), Json::array({ "PropertyClass" }), { { "m_text", Property("std::string", "m_text", 0, 7) } });
        AddClass(classes, std::string(SecondTestClass), Json::array({ "PropertyClass" }), { { "m_text", Property("std::string", "m_text", 0, 7) } });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    inline TypeCatalogPtr Catalog()
    {
        static TypeRegistry registry;
        static bool const loaded = registry.LoadFromText(Dump(), "results.json");
        return loaded ? registry.GetCatalog() : nullptr;
    }

    inline std::vector<uint8> Encode(ResultEntry const& result)
    {
        SerializerOptions options;
        options.Versionable = true;
        options.Mask = 0;
        EncodeResult const encoded = ObjectSerializer::Encode(result.Object.get(), options);
        return encoded.Ok() ? encoded.Bytes : std::vector<uint8>{};
    }

    inline ResultEntry Make(std::string_view className, std::string const& text = {}, uint32 position = 0)
    {
        PropertyObjectPtr object = PropertyObject::Create(Catalog(), className);
        if (object && !text.empty())
            object->Set(className == "class ResAddDynaMod" || className == "class ResRemoveDynaMod" ? "m_dynaModClientTag" : "m_text", std::string(text));
        return ResultEntry{ std::string(className), std::shared_ptr<PropertyObject const>(std::move(object)), position };
    }

    inline ResultEntry Gold(int32 gold, uint32 position = 0)
    {
        PropertyObjectPtr object = PropertyObject::Create(Catalog(), "class ResAddGold");
        if (object)
            object->Set("m_gold", int32{ gold });
        return ResultEntry{ "class ResAddGold", std::shared_ptr<PropertyObject const>(std::move(object)), position };
    }

    inline ResultEntry Experience(int32 experience, std::string const& school, uint32 position = 0)
    {
        PropertyObjectPtr object = PropertyObject::Create(Catalog(), "class ResAddMagicXP");
        if (object)
        {
            object->Set("m_experience", int32{ experience });
            object->Set("m_magicSchool", std::string(school));
        }
        return ResultEntry{ "class ResAddMagicXP", std::shared_ptr<PropertyObject const>(std::move(object)), position };
    }

    inline ResultEntry NotifyText(std::string const& text, int32 type)
    {
        PropertyObjectPtr object = PropertyObject::Create(Catalog(), "class ResClientNotifyText");
        if (object)
        {
            object->Set("m_text", std::string(text));
            object->Set("m_type", int32{ type });
        }
        return ResultEntry{ "class ResClientNotifyText", std::shared_ptr<PropertyObject const>(std::move(object)), 0 };
    }
}

#endif
