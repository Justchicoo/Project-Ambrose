/*
 * Project Ambrose by Imjustchico
 * Writes the zone object classes and the ResSpawn and ResDespawn trigger results as a type dump and loads them, builds the core object and behavior tables over them as the client registers its own, and makes templates and placed rows the way the zone data holds them.
 */

#include "ZoneObjectFixtures.h"
#include "ObjectFields.h"
#include "ObjectViews.h"
#include "StringHash.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <optional>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;
    constexpr uint32 WirePublic = Wire | 4;
    constexpr uint32 Kept = 1 | 2 | 4;

    Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags = WirePublic, std::string const& container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    Json ObjectProperties()
    {
        Json properties = Json::object();
        properties["m_inactiveBehaviors"] = Property("class SharedPointer<class BehaviorInstance>", "m_inactiveBehaviors", 0, WirePublic, "List");
        properties["m_globalID.m_full"] = Property("unsigned __int64", "m_globalID.m_full", 1);
        properties["m_permID"] = Property("unsigned __int64", "m_permID", 2);
        properties["m_location"] = Property("class Vector3D", "m_location", 3);
        properties["m_orientation"] = Property("class Vector3D", "m_orientation", 4);
        properties["m_fScale"] = Property("float", "m_fScale", 5);
        properties["m_templateID.m_full"] = Property("unsigned __int64", "m_templateID.m_full", 6);
        properties["m_nMobileID"] = Property("unsigned short", "m_nMobileID", 7);
        return properties;
    }

    Json TemplateProperties()
    {
        Json properties = Json::object();
        properties["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, Kept, "List");
        properties["m_objectName"] = Property("std::string", "m_objectName", 1, Kept);
        properties["m_templateID"] = Property("unsigned int", "m_templateID", 2, Kept);
        properties["m_visualID"] = Property("unsigned int", "m_visualID", 3, Kept);
        properties["m_adjectiveList"] = Property("std::string", "m_adjectiveList", 4, Kept, "List");
        properties["m_exemptFromAOI"] = Property("bool", "m_exemptFromAOI", 5, Kept);
        properties["m_displayName"] = Property("std::string", "m_displayName", 6, Kept);
        properties["m_description"] = Property("std::string", "m_description", 7, Kept);
        Json type = Property("enum ObjectType", "m_nObjectType", 8, Kept);
        type["enum_options"] = Json{ { "OBJECT_TYPE_UNKNOWN", 0 } };
        properties["m_nObjectType"] = type;
        properties["m_sIcon"] = Property("std::string", "m_sIcon", 9, Kept);
        return properties;
    }

    std::string Dump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "class Vector3D", Json::array(), Json::object());
        AddClass(classes, "enum ObjectType", Json::array(), Json::object());
        AddClass(classes, "class CoreObject", Json::array({ "PropertyClass" }), ObjectProperties());
        AddClass(classes, "class ClientObject", Json::array({ "CoreObject", "PropertyClass" }), ObjectProperties());
        AddClass(classes, "class WizClientObject", Json::array({ "ClientObject", "CoreObject", "PropertyClass" }), ObjectProperties());
        Json instance = Json::object();
        instance["m_behaviorTemplateNameID"] = Property("unsigned int", "m_behaviorTemplateNameID", 0, Kept);
        AddClass(classes, "class BehaviorInstance", Json::array({ "PropertyClass" }), instance);
        AddClass(classes, "class TestRenderBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), instance);
        Json behavior = Json::object();
        behavior["m_behaviorName"] = Property("std::string", "m_behaviorName", 0, Kept);
        AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), behavior);
        Json core = Json::object();
        core["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, Kept, "List");
        AddClass(classes, "class CoreTemplate", Json::array({ "PropertyClass" }), core);
        AddClass(classes, "class GameObjectTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), TemplateProperties());
        AddClass(classes, "class WizGameObjectTemplate", Json::array({ "GameObjectTemplate", "CoreTemplate", "PropertyClass" }), TemplateProperties());
        Json critical = Json::object();
        critical["m_objList"] = Property("gid", "m_objList", 0, WirePublic, "List");
        AddClass(classes, "class CriticalObjectList", Json::array({ "PropertyClass" }), critical);
        AddClass(classes, "class Result", Json::array({ "PropertyClass" }), Json::object());
        Json spawn = Json::object();
        spawn["m_spawnID"] = Property("gid", "m_spawnID", 0, Wire);
        spawn["m_activate"] = Property("bool", "m_activate", 1, Wire);
        AddClass(classes, "class ResSpawn", Json::array({ "Result", "PropertyClass" }), spawn);
        Json despawn = Json::object();
        despawn["m_spawnID"] = Property("gid", "m_spawnID", 0, Wire);
        despawn["m_templateID"] = Property("int", "m_templateID", 1, Wire);
        despawn["m_despawnEffect"] = Property("std::string", "m_despawnEffect", 2, Wire);
        AddClass(classes, "class ResDespawn", Json::array({ "Result", "PropertyClass" }), despawn);
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

}

bool ZoneObjectFixtures::Build(std::string& error)
{
    _views.Add(GameObjectTemplateView::Definition);
    _registry = std::make_unique<TypeRegistry>(&_views);
    if (!_registry->LoadFromText(Dump(), "spawn.json"))
    {
        error = _registry->GetErrors().empty() ? std::string("the dump does not load") : _registry->GetErrors().front();
        return false;
    }
    _catalog = _registry->GetCatalog();
    std::vector<std::string> errors;
    _types = CoreObjectTypeTable::Build({ { 2, "class ClientObject" }, { 104, "class WizClientObject" } },
        { { "class GameObjectTemplate", 2, 2 }, { "class WizGameObjectTemplate", 104, 2 } }, *_catalog, errors);
    if (_types)
        _behaviors = BehaviorClientClasses::Build({ { "RenderBehavior", "class TestRenderBehavior", 0 }, { "DeletedBehavior", std::nullopt, 0 } }, *_catalog, errors);
    if (!_types || !_behaviors)
    {
        error = errors.empty() ? std::string("the tables do not build") : errors.front();
        return false;
    }
    _templates[KioskTemplate] = Template(KioskTemplate, "class WizGameObjectTemplate", { "RenderBehavior", "", "DeletedBehavior" }, {});
    _templates[DoorTemplate] = Template(DoorTemplate, "class GameObjectTemplate", { "RenderBehavior" }, { "Critical" });
    return true;
}

std::shared_ptr<ObjectTemplate const> ZoneObjectFixtures::Template(uint32 id, std::string const& className, std::vector<std::string> behaviors, std::vector<std::string> const& adjectives)
{
    PropertyObjectPtr object = PropertyObject::Create(_catalog, className);
    EXPECT_TRUE(object) << className;
    if (!object)
        return nullptr;
    PropertyValue::List list;
    for (std::string const& adjective : adjectives)
        list.emplace_back(adjective);
    EXPECT_EQ(object->Set("m_adjectiveList", std::move(list)), PropertySetResult::Ok);
    auto made = std::make_shared<ObjectTemplate>();
    made->TemplateId = id;
    made->ObjectName = className;
    made->Behaviors = std::move(behaviors);
    made->Object = std::move(object);
    return made;
}

MapObjectSources ZoneObjectFixtures::Sources()
{
    MapObjectSources sources;
    sources.Catalog = _catalog;
    sources.Types = _types;
    sources.Behaviors = _behaviors;
    sources.Templates = [this](uint32 id) -> TemplateLookup
    {
        auto const found = _templates.find(id);
        if (found == _templates.end())
            return { nullptr, "TemplateManifest.xml does not list it" };
        return { found->second, {} };
    };
    return sources;
}

DecodeResult ZoneObjectFixtures::Decode(MapObject const& object)
{
    ObjectField const* const field = ObjectFields::Find("MSG_NEWOBJECT", "Data");
    EXPECT_NE(field, nullptr);
    SerializerOptions options;
    options.Mask = SerializerOptions::PublicMask;
    return field ? CoreObjectSerializer::DecodeField(_catalog, *field, object.Data, *_types, options) : DecodeResult{};
}

ZoneObjectSpawn ZoneObjectFixtures::Row(uint64 id, uint32 templateId, ZoneObjectLoading loading, PropertyTypes::Vector3D position)
{
    ZoneObjectSpawn row;
    row.Id = id;
    row.ClassName = "class CoreObjectInfo";
    row.TemplateId = templateId;
    row.ObjectId = static_cast<uint32>(100000 + id);
    row.Position = position;
    row.Orientation = { 0.0f, 0.0f, 0.5f };
    row.Scale = 1.25f;
    row.Loading = loading;
    return row;
}
