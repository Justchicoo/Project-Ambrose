/*
 * Project Ambrose by Imjustchico
 * Builds the game object a zone places, the one MSG_NEWOBJECT carries: the class its template's class makes the client build, the CoreObject header of that template, the ids the zone instance gave it, where the zone data stands it, faces it and how large it is, and one behavior for each its template names in the template's own order, built as the class the client makes for that behavior or left empty where the client takes it empty.
 */

#ifndef AMBROSE_ZONEOBJECTBUILDER_H
#define AMBROSE_ZONEOBJECTBUILDER_H

#include "CoreObjectSerializer.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "PropertyValue.h"

#include <string>

struct ZoneObjectPlacement
{
    uint64 GlobalId = 0;
    uint64 PermId = 0;
    uint16 MobileId = 0;
    PropertyTypes::Vector3D Location;
    PropertyTypes::Vector3D Orientation;
    float Scale = 1.0f;
};

class ZoneObjectBuilder
{
public:
    ZoneObjectBuilder() = delete;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, BehaviorClientClasses const& behaviors, ObjectTemplate const& objectTemplate,
        ZoneObjectPlacement const& placement, std::string& problem);
};

#endif
