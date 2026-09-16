/*
 * Project Ambrose by Imjustchico
 * Reads CharacterNames.xml with pugixml as a document of exactly one root element, taking each table's section minus its locale suffix as the .lang stem and a table without a locale in every locale that has that stem, parses each needed .lang file once, joins a value's text around comments and CDATA while refusing elements inside it, and refuses unknown elements, missing sections and keys without text; decodes the disallowed list BINd file through the name views, giving each name its list position as id; reads the creation config's plain XML, whose classes the type dump lacks, by element name, keying schools by their string id; then validates names through CharacterNameSet, requires human names and schools, and lays the rows out for the world tables in table, locale and position order. Every problem goes through one capped report with values from the files escaped.
 */

#include "CharacterNameExtractor.h"
#include "BindFile.h"
#include "KiwadArchive.h"
#include "LangFile.h"
#include "NameViews.h"
#include "StringHash.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <pugixml.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>

namespace
{
    constexpr std::string_view LocaleFolder = "Locale/";
    constexpr std::string_view LangExtension = ".lang";
    constexpr std::string_view ConfigClass = "class WizCharacterCreationConfig";
    constexpr std::string_view SchoolClass = "class AllowedSchoolOption";
    constexpr std::string_view OptionClass = "class AllowedCreationOption";

    std::string Quoted(std::string_view text)
    {
        return Ambrose::ForLog(text);
    }

    class LangFiles
    {
    public:
        using Texts = std::unordered_map<std::string, std::string>;

        explicit LangFiles(KiwadArchive const& archive) : _archive(archive)
        {
        }

        std::vector<std::string> LocalesWith(std::string_view stem) const
        {
            std::string const fileName = fmt::format("{}{}", stem, LangExtension);
            std::set<std::string> locales;
            for (KiwadEntry const& entry : _archive.GetEntries())
            {
                std::string_view name = entry.Name;
                if (!name.starts_with(LocaleFolder))
                    continue;
                name.remove_prefix(LocaleFolder.size());
                std::size_t const slash = name.find('/');
                if (slash == 0 || slash == std::string_view::npos || name.substr(slash + 1) != fileName)
                    continue;
                locales.emplace(name.substr(0, slash));
            }
            return { locales.begin(), locales.end() };
        }

        Texts const* Get(std::string_view locale, std::string_view stem, NameExtraction& extraction)
        {
            std::string const path = fmt::format("{}{}/{}{}", LocaleFolder, locale, stem, LangExtension);
            auto found = _files.find(path);
            if (found != _files.end())
                return found->second ? &*found->second : nullptr;
            std::optional<Texts>& slot = _files[path];
            KiwadReadResult const read = _archive.Read(path, LangFile::MaxFileBytes);
            if (!read.Succeeded())
            {
                extraction.AddError(fmt::format("{}: {}", Quoted(path), read.Error));
                return nullptr;
            }
            LangParseResult parsed = LangFile::Parse(read.Data);
            if (!parsed.Ok())
            {
                extraction.AddError(fmt::format("{}: {}", Quoted(path), parsed.Error));
                return nullptr;
            }
            if (parsed.Stem != stem)
            {
                extraction.AddError(fmt::format("{}: its header names the stem {}, not {}", Quoted(path), Quoted(parsed.Stem), Quoted(stem)));
                return nullptr;
            }
            Texts& texts = slot.emplace();
            for (LangEntry& entry : parsed.Entries)
                texts.insert_or_assign(std::move(entry.Key), std::move(entry.Text));
            return &texts;
        }

    private:
        KiwadArchive const& _archive;
        std::map<std::string, std::optional<Texts>> _files;
    };

    std::unique_ptr<pugi::xml_document> ParseXml(std::span<uint8 const> bytes, std::string_view entry, NameExtraction& extraction)
    {
        auto document = std::make_unique<pugi::xml_document>();
        pugi::xml_parse_result const parsed = document->load_buffer(bytes.data(), bytes.size(), pugi::parse_default | pugi::parse_fragment, pugi::encoding_auto);
        if (!parsed)
        {
            extraction.AddError(fmt::format("{} is not well-formed XML: {} at byte {}", entry, parsed.description(), parsed.offset));
            return nullptr;
        }
        std::size_t elements = 0;
        bool strayText = false;
        for (pugi::xml_node const child : document->children())
        {
            if (child.type() == pugi::node_element)
                ++elements;
            else if (child.type() == pugi::node_cdata || (child.type() == pugi::node_pcdata && !Ambrose::Trim(child.value()).empty()))
                strayText = true;
        }
        if (elements != 1 || strayText)
        {
            extraction.AddError(fmt::format("{} must hold exactly one root element and no text outside it", entry));
            return nullptr;
        }
        return document;
    }

    std::optional<std::string> ReadText(pugi::xml_node node, std::string_view entry, NameExtraction& extraction)
    {
        std::string text;
        for (pugi::xml_node const child : node.children())
        {
            if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
                text += child.value();
            else if (child.type() == pugi::node_element)
            {
                extraction.AddError(fmt::format("{}: <{}> holds an element <{}> where only text belongs", entry, Quoted(node.name()), Quoted(child.name())));
                return std::nullopt;
            }
        }
        return std::string(Ambrose::Trim(text));
    }

    std::vector<pugi::xml_node> Elements(pugi::xml_node parent)
    {
        std::vector<pugi::xml_node> elements;
        for (pugi::xml_node const child : parent.children())
            if (child.type() == pugi::node_element)
                elements.push_back(child);
        return elements;
    }

    std::optional<uint32> ReadOrder(pugi::xml_node node, std::string_view entry, NameExtraction& extraction)
    {
        std::optional<uint32> const order = Ambrose::StringTo<uint32>(node.attribute("key").value());
        if (!order || *order > CharacterNameExtractor::MaxOrder)
        {
            extraction.AddError(fmt::format("{}: <{}> needs a key attribute of 0-{}, not '{}'", entry, Quoted(node.name()), CharacterNameExtractor::MaxOrder, Quoted(node.attribute("key").value())));
            return std::nullopt;
        }
        return order;
    }

    std::optional<pugi::xml_node> ReadOptionClass(pugi::xml_node node, std::string_view className, std::string_view entry, NameExtraction& extraction)
    {
        std::vector<pugi::xml_node> const elements = Elements(node);
        if (elements.size() != 1 || std::string_view(elements.front().name()) != "Class" || className != elements.front().attribute("Name").value())
        {
            extraction.AddError(fmt::format("{}: <{} key=\"{}\"> must hold exactly one <Class Name=\"{}\">", entry, Quoted(node.name()), Quoted(node.attribute("key").value()), className));
            return std::nullopt;
        }
        return elements.front();
    }

    bool IsSchoolName(std::string_view name) noexcept
    {
        return !name.empty() && name.size() <= CharacterNameExtractor::MaxSchoolNameBytes
            && std::all_of(name.begin(), name.end(), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
    }
}

void NameExtraction::AddError(std::string error)
{
    ++ErrorCount;
    if (ErrorCount <= MaxReportedErrors)
        Errors.push_back(std::move(error));
}

void NameExtraction::FinishErrors()
{
    if (ErrorCount > MaxReportedErrors && Errors.size() == MaxReportedErrors)
        Errors.push_back(fmt::format("and {} more problems", ErrorCount - MaxReportedErrors));
}

std::size_t NameExtraction::GetPartCount() const noexcept
{
    std::size_t parts = 0;
    for (CharacterNameTable const& table : Tables)
        parts += table.Parts.size();
    return parts;
}

NameExtraction CharacterNameExtractor::Extract(KiwadArchive const& archive, TypeCatalogPtr const& catalog)
{
    NameExtraction extraction;
    auto const read = [&archive, &extraction](std::string_view entry) -> std::optional<std::vector<uint8>>
    {
        KiwadReadResult result = archive.Read(entry, MaxEntryBytes);
        if (!result.Succeeded())
        {
            extraction.AddError(fmt::format("{}: {}", entry, result.Error));
            return std::nullopt;
        }
        return std::move(result.Data);
    };
    if (std::optional<std::vector<uint8>> const names = read(NamesEntry))
        ReadNameTables(archive, *names, extraction);
    if (std::optional<std::vector<uint8>> const disallowed = read(DisallowedEntry))
        ReadDisallowed(catalog, *disallowed, extraction);
    if (std::optional<std::vector<uint8>> const config = read(CreationConfigEntry))
        ReadCreationConfig(*config, extraction);
    if (extraction.Ok())
        Validate(extraction);
    extraction.FinishErrors();
    return extraction;
}

void CharacterNameExtractor::ReadNameTables(KiwadArchive const& archive, std::span<uint8 const> xml, NameExtraction& extraction)
{
    std::unique_ptr<pugi::xml_document> const document = ParseXml(xml, NamesEntry, extraction);
    if (!document)
        return;
    pugi::xml_node const root = document->document_element();
    if (std::string_view(root.name()) != "CharacterNameTable")
    {
        extraction.AddError(fmt::format("{}: the root element is <{}>, not <CharacterNameTable>", NamesEntry, Quoted(root.name())));
        return;
    }
    LangFiles lang(archive);
    for (pugi::xml_node const table : Elements(root))
    {
        std::string const name = table.attribute("Name").value();
        if (std::string_view(table.name()) != "Table" || name.empty())
        {
            extraction.AddError(fmt::format("{}: expected <Table Name=\"...\">, found <{}>", NamesEntry, Quoted(table.name())));
            continue;
        }
        pugi::xml_attribute const localeAttribute = table.attribute("Locale");
        std::string const locale = localeAttribute.value();
        std::vector<std::string> sections;
        std::vector<std::string> keys;
        bool broken = false;
        for (pugi::xml_node const child : Elements(table))
        {
            std::string_view const element = child.name();
            if (element != "Section" && element != "CharacterName")
            {
                extraction.AddError(fmt::format("{}: table {} holds an unknown element <{}>", NamesEntry, Quoted(name), Quoted(element)));
                broken = true;
                continue;
            }
            std::optional<std::string> text = ReadText(child, NamesEntry, extraction);
            if (!text)
                broken = true;
            else
                (element == "Section" ? sections : keys).push_back(std::move(*text));
        }
        if (broken)
            continue;
        if (sections.size() != 1 || sections.front().empty())
        {
            extraction.AddError(fmt::format("{}: table {} needs exactly one non-empty <Section>, not {}", NamesEntry, Quoted(name), sections.size()));
            continue;
        }
        if (localeAttribute && locale.empty())
        {
            extraction.AddError(fmt::format("{}: table {} has an empty Locale attribute", NamesEntry, Quoted(name)));
            continue;
        }
        std::string stem = sections.front();
        if (!locale.empty() && stem.size() > locale.size() + 1 && stem.ends_with(fmt::format("-{}", locale)))
            stem.resize(stem.size() - locale.size() - 1);
        std::vector<std::string> const locales = locale.empty() ? lang.LocalesWith(stem) : std::vector<std::string>{ locale };
        if (locales.empty())
        {
            extraction.AddError(fmt::format("{}: table {} uses section {}, but no locale holds {}.lang", NamesEntry, Quoted(name), Quoted(sections.front()), Quoted(stem)));
            continue;
        }
        for (std::string const& tableLocale : locales)
        {
            LangFiles::Texts const* const texts = lang.Get(tableLocale, stem, extraction);
            if (!texts)
                continue;
            CharacterNameTable& extracted = extraction.Tables.emplace_back();
            extracted.Name = name;
            extracted.Locale = tableLocale;
            extracted.Parts.reserve(keys.size());
            for (std::size_t index = 0; index < keys.size(); ++index)
            {
                CharacterNamePart& part = extracted.Parts.emplace_back();
                part.LocaleKey = keys[index];
                if (part.LocaleKey.empty())
                    continue;
                auto const text = texts->find(part.LocaleKey);
                if (text != texts->end())
                    part.Text = text->second;
                else
                    extraction.AddError(fmt::format("{}: table {} ({}) position {}: {}{}/{}{} has no key {}", NamesEntry, Quoted(name), Quoted(tableLocale), index, LocaleFolder, Quoted(tableLocale), Quoted(stem), LangExtension, Quoted(part.LocaleKey)));
            }
        }
    }
}

void CharacterNameExtractor::ReadDisallowed(TypeCatalogPtr const& catalog, std::span<uint8 const> bind, NameExtraction& extraction)
{
    BindReadResult const read = BindFile::Read(catalog, bind);
    if (!read.Ok())
    {
        extraction.AddError(fmt::format("{}: {}: {}", DisallowedEntry, BindFile::GetStatusName(read.Status), read.Detail));
        return;
    }
    for (DecodeIssue const& issue : read.Decoded.Issues)
        extraction.AddError(fmt::format("{}: {} at {}: {}", DisallowedEntry, ObjectSerializer::GetIssueName(issue.Kind), issue.Path, issue.Detail));
    if (!read.Decoded.Issues.empty())
        return;
    std::optional<DisallowedNameListView> const list = DisallowedNameListView::From(read.Decoded.Object.get());
    if (!list)
    {
        extraction.AddError(fmt::format("{} holds {}, which the disallowed name list view does not read; the type dump must list class DisallowedNameList", DisallowedEntry, read.Decoded.Object ? Quoted(read.Decoded.Object->GetClass().Name) : std::string("no object")));
        return;
    }
    uint32 id = 0;
    for (PropertyValue const& value : list->GetNames())
    {
        std::optional<DisallowedNameView> const name = DisallowedNameView::From(value.AsObject());
        if (!name)
            extraction.AddError(fmt::format("{}: entry {} is not a DisallowedName", DisallowedEntry, id));
        else
            extraction.Disallowed.push_back(DisallowedName{ id, name->GetLocale(), name->GetGender(), name->GetFirst(), name->GetMiddle(), name->GetLast() });
        ++id;
    }
}

void CharacterNameExtractor::ReadCreationConfig(std::span<uint8 const> xml, NameExtraction& extraction)
{
    std::unique_ptr<pugi::xml_document> const document = ParseXml(xml, CreationConfigEntry, extraction);
    if (!document)
        return;
    pugi::xml_node const root = document->document_element();
    std::vector<pugi::xml_node> const classes = Elements(root);
    if (std::string_view(root.name()) != "Objects" || classes.size() != 1 || std::string_view(classes.front().name()) != "Class" || ConfigClass != classes.front().attribute("Name").value())
    {
        extraction.AddError(fmt::format("{} must be <Objects> holding one <Class Name=\"{}\">", CreationConfigEntry, ConfigClass));
        return;
    }
    std::set<uint32> schoolOrders;
    std::set<uint32> optionOrders;
    std::set<std::string> schoolNames;
    std::map<uint32, std::string> schoolIds;
    for (pugi::xml_node const property : Elements(classes.front()))
    {
        std::string_view const element = property.name();
        if (element != "m_schoolOptions" && element != "m_creationOptions")
        {
            extraction.AddError(fmt::format("{}: {} holds an unknown property <{}>", CreationConfigEntry, ConfigClass, Quoted(element)));
            continue;
        }
        bool const school = element == "m_schoolOptions";
        std::optional<uint32> const order = ReadOrder(property, CreationConfigEntry, extraction);
        std::optional<pugi::xml_node> const option = ReadOptionClass(property, school ? SchoolClass : OptionClass, CreationConfigEntry, extraction);
        if (!order || !option)
            continue;
        if (!(school ? schoolOrders : optionOrders).insert(*order).second)
        {
            extraction.AddError(fmt::format("{}: <{}> key {} is used more than once", CreationConfigEntry, element, *order));
            continue;
        }
        std::vector<pugi::xml_node> const fields = Elements(*option);
        std::string_view const wanted = school ? "m_schoolName" : "m_templateID";
        if (fields.size() != 1 || wanted != fields.front().name())
        {
            extraction.AddError(fmt::format("{}: <{} key=\"{}\"> must hold only <{}>", CreationConfigEntry, element, *order, wanted));
            continue;
        }
        std::optional<std::string> const value = ReadText(fields.front(), CreationConfigEntry, extraction);
        if (!value)
            continue;
        if (!school)
        {
            std::optional<uint32> const templateId = Ambrose::StringTo<uint32>(*value);
            if (!templateId)
                extraction.AddError(fmt::format("{}: <m_creationOptions key=\"{}\"> has the template id '{}', which is not a number", CreationConfigEntry, *order, Quoted(*value)));
            else
                extraction.Options.push_back(CreationOption{ *order, *templateId });
            continue;
        }
        if (!IsSchoolName(*value))
        {
            extraction.AddError(fmt::format("{}: <m_schoolOptions key=\"{}\"> has the school name '{}'; a school name is 1-{} letters, digits or underscores", CreationConfigEntry, *order, Quoted(*value), MaxSchoolNameBytes));
            continue;
        }
        uint32 const id = StringHash::StringId(*value);
        auto const [clash, added] = schoolIds.try_emplace(id, *value);
        if (!schoolNames.emplace(*value).second || !added)
        {
            extraction.AddError(fmt::format("{}: the school {} is listed twice or shares its string id {} with {}", CreationConfigEntry, *value, id, clash->second));
            continue;
        }
        extraction.Schools.push_back(CreationSchool{ *order, *value, id });
    }
    std::sort(extraction.Schools.begin(), extraction.Schools.end(), [](CreationSchool const& left, CreationSchool const& right) { return left.Order < right.Order; });
    std::sort(extraction.Options.begin(), extraction.Options.end(), [](CreationOption const& left, CreationOption const& right) { return left.Order < right.Order; });
}

void CharacterNameExtractor::Validate(NameExtraction& extraction)
{
    std::vector<std::string> problems;
    std::shared_ptr<CharacterNameSet const> const names = CharacterNameSet::Build(extraction.Tables, extraction.Disallowed, problems);
    for (std::string& problem : problems)
        extraction.AddError(std::move(problem));
    if (names && names->GetHumanLocales().empty())
        extraction.AddError(fmt::format("{} holds no locale with all four human name tables", NamesEntry));
    if (extraction.Schools.empty())
        extraction.AddError(fmt::format("{} offers no school", CreationConfigEntry));
}

WorldSqlScript CharacterNameExtractor::BuildScript(NameExtraction const& extraction)
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

std::vector<std::string_view> CharacterNameExtractor::GetTables()
{
    return { "character_name_part", "character_name_disallowed", "character_create_school", "character_create_option" };
}
