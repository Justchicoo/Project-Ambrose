/*
 * Project Ambrose by Imjustchico
 * Lays extracted templates out for the world tables in id order, replacing object_template first so its adjectives and behaviors are only ever written after it, with NULL for a visual id or object type a template does not have and for the name of a behavior of a class nothing describes. Replacing every table whole makes a second run over the same install write the same rows.
 */

#include "TemplateScript.h"

WorldSqlScript TemplateScript::Build(TemplateExtraction const& extraction)
{
    auto const optional = [](auto const& value)
    {
        return value ? WorldSqlScript::Value{ static_cast<int64>(*value) } : WorldSqlScript::Value{ std::monostate{} };
    };
    std::vector<WorldSqlScript::Row> templates;
    std::vector<WorldSqlScript::Row> adjectives;
    std::vector<WorldSqlScript::Row> behaviors;
    templates.reserve(extraction.Templates.size());
    adjectives.reserve(extraction.GetAdjectiveCount());
    behaviors.reserve(extraction.GetBehaviorCount());
    for (ExtractedTemplate const& found : extraction.Templates)
    {
        uint64 const id = found.TemplateId;
        templates.push_back({ id, found.ClassName, uint64{ found.ClassHash }, found.Archive, found.Path, found.ObjectName, found.DisplayKey, found.DescriptionKey, found.Icon,
            optional(found.VisualId), optional(found.ObjectType), found.LootTable });
        for (std::size_t position = 0; position < found.Adjectives.size(); ++position)
            adjectives.push_back({ id, uint64{ position }, found.Adjectives[position] });
        for (std::size_t position = 0; position < found.Behaviors.size(); ++position)
        {
            ExtractedBehavior const& behavior = found.Behaviors[position];
            behaviors.push_back({ id, uint64{ position }, uint64{ behavior.ClassHash },
                behavior.Name ? WorldSqlScript::Value{ *behavior.Name } : WorldSqlScript::Value{ std::monostate{} } });
        }
    }

    std::vector<std::string_view> const tables = GetTables();
    WorldSqlScript script;
    script.ReplaceTable(tables[0], { "template_id", "class_name", "class_hash", "archive", "source_path", "object_name", "display_key", "description_key", "icon", "visual_id",
        "object_type", "loot_table" }, templates);
    script.ReplaceTable(tables[1], { "template_id", "position", "adjective" }, adjectives);
    script.ReplaceTable(tables[2], { "template_id", "position", "class_hash", "behavior_name" }, behaviors);
    return script;
}

std::vector<std::string_view> TemplateScript::GetTables()
{
    return { "object_template", "object_template_adjective", "object_template_behavior" };
}
