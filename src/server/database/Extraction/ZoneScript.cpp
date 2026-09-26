/*
 * Project Ambrose by Imjustchico
 * Lays extracted zones out for the world tables in the order they were read: one zone_template row per zone, then its locations and its objects in list order, replacing zone_template first so the rows that name a zone are only ever written after it, with every float carried as the double it widens to and spawn requirements NULL where an object has none.
 */

#include "ZoneScript.h"

WorldSqlScript ZoneScript::Build(ZoneExtraction const& extraction)
{
    auto const number = [](float value) { return WorldSqlScript::Value{ static_cast<double>(value) }; };
    auto const whole = [](int64 value) { return WorldSqlScript::Value{ value }; };
    auto const flag = [](bool value) { return WorldSqlScript::Value{ uint64{ value ? 1u : 0u } }; };

    std::vector<WorldSqlScript::Row> templates;
    std::vector<WorldSqlScript::Row> locations;
    std::vector<WorldSqlScript::Row> objects;
    templates.reserve(extraction.Zones.size());
    locations.reserve(extraction.GetLocationCount());
    objects.reserve(extraction.GetObjectCount());
    for (ExtractedZone const& zone : extraction.Zones)
    {
        templates.push_back({ zone.Path, zone.DisplayNameKey, number(zone.FarClip), whole(zone.HealingPerMinute), whole(zone.SoftLimit), whole(zone.HardLimit), flag(zone.NoMounts) });
        for (ExtractedLocation const& location : zone.Locations)
            locations.push_back({ zone.Path, location.Name, number(location.Location.X), number(location.Location.Y), number(location.Location.Z), number(location.Direction) });
        for (ExtractedObject const& object : zone.Objects)
            objects.push_back({ zone.Path, object.ClassName, object.TemplateId, uint64{ object.ObjectId }, number(object.Location.X), number(object.Location.Y), number(object.Location.Z),
                number(object.Orientation.X), number(object.Orientation.Y), number(object.Orientation.Z), number(object.Scale), object.ZoneTag, object.StartState, object.OverrideName,
                flag(object.GlobalDynamic), flag(object.Undetectable), whole(object.LoadingType),
                object.SpawnRequirements ? WorldSqlScript::Value{ std::string(object.SpawnRequirements->begin(), object.SpawnRequirements->end()) }
                                         : WorldSqlScript::Value{ std::monostate{} } });
    }

    std::vector<std::string_view> const tables = GetTables();
    WorldSqlScript script;
    script.ReplaceTable(tables[0], { "zone_path", "display_name_key", "far_clip", "healing_per_minute", "soft_limit", "hard_limit", "no_mounts" }, templates);
    script.ReplaceTable(tables[1], { "zone_path", "name", "position_x", "position_y", "position_z", "direction" }, locations);
    script.ReplaceTable(tables[2], { "zone_path", "class_name", "template_id", "object_id", "position_x", "position_y", "position_z", "orientation_x", "orientation_y", "orientation_z",
        "scale", "zone_tag", "start_state", "override_name", "global_dynamic", "undetectable", "loading_type", "spawn_requirements" }, objects);
    return script;
}

std::vector<std::string_view> ZoneScript::GetTables()
{
    return { "zone_template", "zone_location", "zone_object" };
}
