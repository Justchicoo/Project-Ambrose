/*
 * Project Ambrose by Imjustchico
 * Lays extracted rows out for the world tables in table, locale and position order for name parts, list order for disallowed names, and key order for schools and creation options.
 */

#include "CharacterNameScript.h"

WorldSqlScript CharacterNameScript::Build(NameExtraction const& extraction)
{
    std::vector<WorldSqlScript::Row> parts;
    parts.reserve(extraction.GetPartCount());
    for (CharacterNameTable const& table : extraction.Tables)
        for (std::size_t index = 0; index < table.Parts.size(); ++index)
            parts.push_back({ table.Name, table.Locale, uint64{ index }, table.Parts[index].LocaleKey, table.Parts[index].Text });
    std::vector<WorldSqlScript::Row> disallowed;
    for (DisallowedName const& name : extraction.Disallowed)
        disallowed.push_back({ uint64{ name.Id }, uint64{ name.LocaleId }, uint64{ name.Gender }, uint64{ name.First }, uint64{ name.Middle }, uint64{ name.Last } });
    std::vector<WorldSqlScript::Row> schools;
    for (CreationSchool const& school : extraction.Schools)
        schools.push_back({ uint64{ school.Id }, school.Name, uint64{ school.Order } });
    std::vector<WorldSqlScript::Row> options;
    for (CreationOption const& option : extraction.Options)
        options.push_back({ uint64{ option.Order }, uint64{ option.TemplateId } });

    std::vector<std::string_view> const tables = GetTables();
    WorldSqlScript script;
    script.ReplaceTable(tables[0], { "table_name", "locale", "idx", "locale_key", "text" }, parts);
    script.ReplaceTable(tables[1], { "id", "locale_id", "gender", "first_idx", "middle_idx", "last_idx" }, disallowed);
    script.ReplaceTable(tables[2], { "school_id", "school_name", "sort_order" }, schools);
    script.ReplaceTable(tables[3], { "sort_order", "template_id" }, options);
    return script;
}

std::vector<std::string_view> CharacterNameScript::GetTables()
{
    return { "character_name_part", "character_name_disallowed", "character_create_school", "character_create_option" };
}
