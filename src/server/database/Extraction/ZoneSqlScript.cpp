/*
 * Project Ambrose by Imjustchico
 * Lays extracted zones out for the world tables in the order they were read: one zone_template row per zone, then its locations, objects, volumes and triggers in list order, each volume and trigger known by its place in its list, with every event of each in order and every result of each trigger, replacing zone_template first so the rows that name a zone are only ever written after it, and zone_trigger before its results, with every float carried as the double it widens to and requirements, placed objects and result bytes NULL where there are none.
 */

#include "ZoneSqlScript.h"

WorldSqlScript ZoneSqlScript::Build(ZoneExtraction const& extraction)
{
    auto const number = [](float value) { return WorldSqlScript::Value{ static_cast<double>(value) }; };
    auto const whole = [](int64 value) { return WorldSqlScript::Value{ value }; };
    auto const flag = [](bool value) { return WorldSqlScript::Value{ uint64{ value ? 1u : 0u } }; };

    std::vector<WorldSqlScript::Row> templates;
    std::vector<WorldSqlScript::Row> locations;
    std::vector<WorldSqlScript::Row> objects;
    std::vector<WorldSqlScript::Row> volumes;
    std::vector<WorldSqlScript::Row> triggers;
    std::vector<WorldSqlScript::Row> events;
    std::vector<WorldSqlScript::Row> results;
    auto const bytes = [](std::optional<std::vector<uint8>> const& data)
    {
        return data ? WorldSqlScript::Value{ std::string(data->begin(), data->end()) } : WorldSqlScript::Value{ std::monostate{} };
    };
    auto const addEvents = [&events](std::string const& zone, std::string_view owner, std::size_t index, std::string_view kind, std::vector<std::string> const& names)
    {
        for (std::size_t position = 0; position < names.size(); ++position)
            events.push_back({ zone, std::string(owner), uint64{ index }, std::string(kind), uint64{ position }, names[position] });
    };
    auto const addResults = [&results, &bytes](std::string const& zone, std::size_t index, std::string_view list, std::vector<ExtractedTriggerResult> const& rows)
    {
        for (std::size_t position = 0; position < rows.size(); ++position)
            results.push_back({ zone, uint64{ index }, std::string(list), uint64{ position }, uint64{ rows[position].ClassHash },
                rows[position].ClassName ? WorldSqlScript::Value{ *rows[position].ClassName } : WorldSqlScript::Value{ std::monostate{} }, bytes(rows[position].Data) });
    };
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
        for (std::size_t index = 0; index < zone.Volumes.size(); ++index)
        {
            ExtractedVolume const& volume = zone.Volumes[index];
            volumes.push_back({ zone.Path, uint64{ index }, volume.Name, uint64{ volume.ObjectId }, volume.TemplateId, volume.Shape, number(volume.Position.X), number(volume.Position.Y),
                number(volume.Position.Z), number(volume.Radius), number(volume.Length), number(volume.Width), number(volume.Depth), flag(volume.QuestEvents), flag(volume.PlayerOnly),
                whole(volume.LoadingType), bytes(volume.SpawnRequirements) });
            addEvents(zone.Path, "volume", index, "enter", volume.EnterEvents);
            addEvents(zone.Path, "volume", index, "exit", volume.ExitEvents);
        }
        for (std::size_t index = 0; index < zone.Triggers.size(); ++index)
        {
            ExtractedTrigger const& trigger = zone.Triggers[index];
            std::optional<ExtractedStateTrigger> const& state = trigger.State;
            auto const stateText = [&state](std::string ExtractedStateTrigger::* field) { return state ? WorldSqlScript::Value{ (*state).*field } : WorldSqlScript::Value{ std::monostate{} }; };
            auto const stateFlag = [&state, &flag](bool ExtractedStateTrigger::* field) { return state ? flag((*state).*field) : WorldSqlScript::Value{ std::monostate{} }; };
            triggers.push_back({ zone.Path, uint64{ index }, trigger.Name, trigger.ClassName, whole(trigger.TriggerMax), number(trigger.Cooldown), whole(trigger.Unnamed780900737),
                flag(trigger.Unnamed847435658), trigger.Unnamed1549045087, trigger.Unnamed2293879431, bytes(trigger.Requirements), bytes(trigger.ObjectInfo),
                stateText(&ExtractedStateTrigger::QuestEvent), stateText(&ExtractedStateTrigger::RequiredQuest), stateText(&ExtractedStateTrigger::RequiredState),
                stateFlag(&ExtractedStateTrigger::Unnamed333662217), stateFlag(&ExtractedStateTrigger::Unnamed758563334), stateText(&ExtractedStateTrigger::Unnamed3350245995),
                stateText(&ExtractedStateTrigger::Unnamed3431571632) });
            addEvents(zone.Path, "trigger", index, "activate", trigger.ActivateEvents);
            addEvents(zone.Path, "trigger", index, "fire", trigger.FireEvents);
            addEvents(zone.Path, "trigger", index, "deactivate", trigger.DeactivateEvents);
            if (state)
                addEvents(zone.Path, "trigger", index, "unnamed_1521843245", state->Unnamed1521843245);
            addResults(zone.Path, index, "results", trigger.Results);
            addResults(zone.Path, index, "cooldown", trigger.CooldownResults);
        }
    }

    std::vector<std::string_view> const tables = GetTables();
    WorldSqlScript script;
    script.ReplaceTable(tables[0], { "zone_path", "display_name_key", "far_clip", "healing_per_minute", "soft_limit", "hard_limit", "no_mounts" }, templates);
    script.ReplaceTable(tables[1], { "zone_path", "name", "position_x", "position_y", "position_z", "direction" }, locations);
    script.ReplaceTable(tables[2], { "zone_path", "class_name", "template_id", "object_id", "position_x", "position_y", "position_z", "orientation_x", "orientation_y", "orientation_z",
        "scale", "zone_tag", "start_state", "override_name", "global_dynamic", "undetectable", "loading_type", "spawn_requirements" }, objects);
    script.ReplaceTable(tables[3], { "zone_path", "volume_index", "name", "object_id", "template_id", "shape", "position_x", "position_y", "position_z", "radius", "length", "width", "depth",
        "quest_events", "player_only", "loading_type", "spawn_requirements" }, volumes);
    script.ReplaceTable(tables[4], { "zone_path", "trigger_index", "name", "class_name", "trigger_max", "cooldown", "unnamed_780900737", "unnamed_847435658", "unnamed_1549045087",
        "unnamed_2293879431", "requirements", "object_info", "quest_event", "required_quest", "required_state", "unnamed_333662217", "unnamed_758563334", "unnamed_3350245995",
        "unnamed_3431571632" }, triggers);
    script.ReplaceTable(tables[5], { "zone_path", "owner", "owner_index", "kind", "position", "event_name" }, events);
    script.ReplaceTable(tables[6], { "zone_path", "trigger_index", "list", "position", "class_hash", "class_name", "data" }, results);
    return script;
}

std::vector<std::string_view> ZoneSqlScript::GetTables()
{
    return { "zone_template", "zone_location", "zone_object", "zone_volume", "zone_trigger", "zone_trigger_event", "zone_trigger_result" };
}
