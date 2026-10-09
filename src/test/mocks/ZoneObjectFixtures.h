/*
 * Project Ambrose by Imjustchico
 * The game object and template classes a zone instance builds its objects from, laid out the way the client's dump gives them, for tests that place objects without a client: CoreObject, ClientObject and WizClientObject with the properties MSG_NEWOBJECT carries, a behavior instance and template, GameObjectTemplate and WizGameObjectTemplate, CriticalObjectList, and the ResSpawn and ResDespawn trigger results; with the core object and behavior tables built over them, a kiosk template and a door template marked Critical, the sources an instance places objects through, and the decode of the Data an object travels as.
 */

#ifndef AMBROSE_ZONEOBJECTFIXTURES_H
#define AMBROSE_ZONEOBJECTFIXTURES_H

#include "CoreObjectSerializer.h"
#include "Map.h"
#include "MapObjectSpawner.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class ZoneObjectFixtures
{
public:
    static constexpr char const* Hub = "WizardCity/WC_Hub";
    static constexpr uint32 KioskTemplate = 4336;
    static constexpr uint32 DoorTemplate = 1451036;

    bool Build(std::string& error);
    std::shared_ptr<ObjectTemplate const> Template(uint32 id, std::string const& className, std::vector<std::string> behaviors, std::vector<std::string> const& adjectives);
    MapObjectSources Sources();
    DecodeResult Decode(MapObject const& object);
    static ZoneObjectSpawn Row(uint64 id, uint32 templateId, ZoneObjectLoading loading, PropertyTypes::Vector3D position);

    TypedViewRegistry _views;
    std::unique_ptr<TypeRegistry> _registry;
    TypeCatalogPtr _catalog;
    CoreObjectTypeTablePtr _types;
    std::shared_ptr<BehaviorClientClasses const> _behaviors;
    std::map<uint32, std::shared_ptr<ObjectTemplate const>> _templates;
};

#endif
