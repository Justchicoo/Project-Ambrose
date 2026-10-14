/*
 * Project Ambrose by Imjustchico
 * A type dump and catalog holding the zone token classes tests run, ZoneTokenContainer, ZoneTokenBoolean, ZoneTokenCount and ZoneTokenTimer with the server-only ResZoneTokenEnable, ResZoneTokenDisable and ResZoneTokenModify, with tokens built ready to read and containers encoded as zone_object holds them.
 */

#ifndef AMBROSE_ZONETOKENFIXTURES_H
#define AMBROSE_ZONETOKENFIXTURES_H

#include "CharacterTypeFixtures.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "TypeRegistry.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ZoneTokenFixtures
{
    constexpr int64 PlayerStorage = 0;
    constexpr int64 ZoneStorage = 1;

    inline std::string Dump()
    {
        using CharacterTypeFixtures::Detail::AddClass;
        using CharacterTypeFixtures::Detail::Json;
        using CharacterTypeFixtures::Detail::Property;
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), {});
        AddClass(classes, "enum ZTStorage", Json::array(), {});
        Json storage = Property("enum ZTStorage", "m_storage", 1, 7);
        storage["enum_options"] = Json{ { "ZTS_Player", PlayerStorage }, { "ZTS_Zone", ZoneStorage } };
        std::vector<std::pair<std::string, Json>> const base{ { "m_debugName", Property("std::string", "m_debugName", 0, 7) }, { "m_storage", storage }, { "m_enabled", Property("bool", "m_enabled", 2, 0x1F) } };
        AddClass(classes, "class ZoneTokenBase", Json::array({ "PropertyClass" }), base);
        std::vector<std::pair<std::string, Json>> boolean = base;
        boolean.emplace_back("m_bSet", Property("bool", "m_bSet", 3, 0x1F));
        AddClass(classes, "class ZoneTokenBoolean", Json::array({ "ZoneTokenBase", "PropertyClass" }), boolean);
        std::vector<std::pair<std::string, Json>> count = base;
        count.emplace_back("m_minValue", Property("int", "m_minValue", 3, 7));
        count.emplace_back("m_startValue", Property("int", "m_startValue", 4, 7));
        count.emplace_back("m_maxValue", Property("int", "m_maxValue", 5, 7));
        count.emplace_back("m_curValue", Property("int", "m_curValue", 6, 0x1F));
        AddClass(classes, "class ZoneTokenCount", Json::array({ "ZoneTokenBase", "PropertyClass" }), count);
        std::vector<std::pair<std::string, Json>> timer = base;
        timer.emplace_back("m_duration", Property("std::string", "m_duration", 3, 7));
        AddClass(classes, "class ZoneTokenTimer", Json::array({ "ZoneTokenBase", "PropertyClass" }), timer);
        AddClass(classes, "class ZoneTokenContainer", Json::array({ "PropertyClass" }), { { "m_tokens", Property("class ZoneTokenBase*", "m_tokens", 0, 7, "List") } });
        std::vector<std::pair<std::string, Json>> const change{ { "m_tokenName", Property("std::string", "m_tokenName", 0, 7) }, { "m_zoneWide", Property("bool", "m_zoneWide", 1, 7) } };
        AddClass(classes, "class ResZoneTokenEnable", Json::array({ "PropertyClass" }), change);
        AddClass(classes, "class ResZoneTokenDisable", Json::array({ "PropertyClass" }), change);
        std::vector<std::pair<std::string, Json>> modify = change;
        modify.emplace_back("m_delta", Property("int", "m_delta", 2, 7));
        AddClass(classes, "class ResZoneTokenModify", Json::array({ "PropertyClass" }), modify);
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    inline TypeCatalogPtr Catalog()
    {
        static TypeRegistry registry;
        static bool const loaded = registry.LoadFromText(Dump(), "tokens.json");
        return loaded ? registry.GetCatalog() : nullptr;
    }

    inline PropertyObjectPtr Boolean(std::string const& name, int64 storage = PlayerStorage)
    {
        PropertyObjectPtr token = PropertyObject::Create(Catalog(), "class ZoneTokenBoolean");
        if (token)
        {
            token->Set("m_debugName", std::string(name));
            token->Set("m_storage", int64{ storage });
        }
        return token;
    }

    inline PropertyObjectPtr Count(std::string const& name, int32 minimum, int32 start, int32 maximum, int64 storage = PlayerStorage)
    {
        PropertyObjectPtr token = PropertyObject::Create(Catalog(), "class ZoneTokenCount");
        if (token)
        {
            token->Set("m_debugName", std::string(name));
            token->Set("m_storage", int64{ storage });
            token->Set("m_minValue", int32{ minimum });
            token->Set("m_startValue", int32{ start });
            token->Set("m_maxValue", int32{ maximum });
            token->Set("m_curValue", int32{ start });
        }
        return token;
    }

    template <typename... Tokens>
    inline PropertyObjectPtr Container(Tokens&&... tokens)
    {
        PropertyObjectPtr container = PropertyObject::Create(Catalog(), "class ZoneTokenContainer");
        PropertyValue::List held;
        (held.emplace_back(std::forward<Tokens>(tokens)), ...);
        if (container)
            container->Set("m_tokens", std::move(held));
        return container;
    }

    inline std::vector<uint8> Encode(PropertyObject const* object)
    {
        SerializerOptions options;
        options.Versionable = true;
        options.Mask = 0;
        EncodeResult const encoded = ObjectSerializer::Encode(object, options);
        return encoded.Ok() ? encoded.Bytes : std::vector<uint8>{};
    }

    inline PropertyObjectPtr Change(std::string_view className, std::string const& token, int32 delta = 0)
    {
        PropertyObjectPtr result = PropertyObject::Create(Catalog(), className);
        if (result)
        {
            result->Set("m_tokenName", std::string(token));
            result->Set("m_zoneWide", true);
            if (className == "class ResZoneTokenModify")
                result->Set("m_delta", int32{ delta });
        }
        return result;
    }
}

#endif
