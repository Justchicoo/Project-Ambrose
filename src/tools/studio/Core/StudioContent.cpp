/*
 * Project Ambrose by Imjustchico
 * Builds the Studio's answers from the quest rows, the install's Root.wad and the type dump: pages filtered by a case-blind search, quests with their locale text, blobs decoded through the zone data options, cinematic entries decoded as BINd, a headerless versionable object or plain XML, and an index from each cinematic's name to the quest results that play it.
 */

#include "StudioContent.h"

#include "BindFile.h"
#include "ObjectSerializer.h"
#include "PropertyJson.h"
#include "XmlObjectReader.h"
#include "ZoneExtractor.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace
{
    std::string Lower(std::string_view text)
    {
        std::string lowered(text);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lowered;
    }

    bool Matches(std::string_view search, std::initializer_list<std::string_view> fields)
    {
        if (search.empty())
            return true;
        std::string const wanted = Lower(search);
        return std::any_of(fields.begin(), fields.end(), [&wanted](std::string_view field) { return Lower(field).find(wanted) != std::string::npos; });
    }

    template <typename Row, typename Key>
    std::vector<Row const*> RowsOf(std::vector<Row> const& rows, Key const& key)
    {
        std::vector<Row const*> found;
        for (Row const& row : rows)
            if (key(row))
                found.push_back(&row);
        return found;
    }

    void CollectStrings(nlohmann::ordered_json const& value, std::vector<std::string>& out)
    {
        if (value.is_string())
            out.push_back(value.get<std::string>());
        else if (value.is_object() || value.is_array())
            for (auto const& item : value)
                CollectStrings(item, out);
    }
}

StudioContent::StudioContent(QuestRows rows, TypeCatalogPtr catalog, std::shared_ptr<KiwadArchive const> root, TextLookup text)
    : _rows(std::move(rows)), _catalog(std::move(catalog)), _root(std::move(root)), _text(std::move(text))
{
    std::sort(_rows.Quests.begin(), _rows.Quests.end(), [](QuestTemplateRow const& left, QuestTemplateRow const& right) { return left.Name < right.Name; });
    if (_root)
        for (KiwadEntry const& entry : _root->GetEntries())
            if (Lower(entry.Name).starts_with(Lower(CinematicFolder)))
            {
                _cinematics.push_back(entry.Name);
                _cinematicByKey.emplace(CinematicKey(entry.Name), entry.Name);
            }
    std::sort(_cinematics.begin(), _cinematics.end());
    IndexCinematics();
}

std::string StudioContent::CinematicKey(std::string_view text)
{
    std::size_t const slash = text.find_last_of("/\\");
    if (slash != std::string_view::npos)
        text.remove_prefix(slash + 1);
    std::string key = Lower(text);
    if (key.ends_with(".xml"))
        key.resize(key.size() - 4);
    return key;
}

nlohmann::ordered_json StudioContent::Decode(std::vector<uint8> const& bytes) const
{
    if (bytes.empty())
        return nullptr;
    if (!_catalog)
        return { { "undecoded", bytes.size() } };
    DecodeResult const decoded = ObjectSerializer::Decode(_catalog, bytes, ZoneExtractor::DataOptions());
    if (!decoded.Ok() || !decoded.Object)
        return { { "undecoded", bytes.size() }, { "detail", decoded.Detail } };
    return PropertyJson::ToJson(decoded.Object.get());
}

nlohmann::ordered_json StudioContent::DecodeEntry(std::vector<uint8> const& bytes, std::string& error) const
{
    if (BindFile::IsBind(bytes))
    {
        BindReadResult const read = BindFile::Read(_catalog, bytes, std::nullopt, true);
        if (read.Ok() && read.Decoded.Object)
            return PropertyJson::ToJson(read.Decoded.Object.get());
        error = read.Detail;
        return nullptr;
    }
    std::string_view const text(reinterpret_cast<char const*>(bytes.data()), bytes.size());
    if (text.find("<Objects") != std::string_view::npos)
    {
        XmlReadResult const read = XmlObjectReader::Read(_catalog, text);
        if (read.Ok() && !read.Objects.empty())
        {
            if (read.Objects.size() == 1)
                return PropertyJson::ToJson(read.Objects.front().get());
            nlohmann::ordered_json objects = nlohmann::ordered_json::array();
            for (PropertyObjectPtr const& object : read.Objects)
                objects.push_back(PropertyJson::ToJson(object.get()));
            return objects;
        }
        error = read.Detail;
        return nullptr;
    }
    SerializerOptions options;
    options.Versionable = true;
    options.Flags = SerializerFlag::None;
    options.Mask = 0;
    options.Limits = BindFile::GetDefaultLimits();
    options.KeepSkippedValues = true;
    DecodeResult const raw = ObjectSerializer::Decode(_catalog, bytes, options);
    if (raw.Ok() && raw.Object)
        return PropertyJson::ToJson(raw.Object.get());
    error = raw.Detail.empty() ? "the entry is neither BINd, plain XML nor a versionable object" : raw.Detail;
    return nullptr;
}

std::string StudioContent::Text(std::string const& key) const
{
    if (key.empty() || !_text)
        return std::string();
    return _text(key).value_or(std::string());
}

std::vector<std::string> StudioContent::NamedCinematics(nlohmann::ordered_json const& value) const
{
    std::vector<std::string> strings;
    CollectStrings(value, strings);
    std::set<std::string> found;
    for (std::string const& text : strings)
        if (auto const it = _cinematicByKey.find(CinematicKey(text)); it != _cinematicByKey.end())
            found.insert(it->second);
    return { found.begin(), found.end() };
}

void StudioContent::IndexCinematics()
{
    for (QuestResultRow const& result : _rows.Results)
        for (std::string const& cinematic : NamedCinematics(Decode(result.Data)))
            _uses[cinematic].push_back({ result.Quest, result.Goal, result.Class, result.Start });
}

nlohmann::ordered_json StudioContent::Summary() const
{
    return { { "quests", _rows.Quests.size() }, { "goals", _rows.Goals.size() }, { "results", _rows.Results.size() }, { "cinematics", _cinematics.size() },
        { "linked_cinematics", _uses.size() }, { "install", _root != nullptr }, { "types", _catalog != nullptr } };
}

nlohmann::ordered_json StudioContent::Quests(std::string_view search, std::size_t offset, std::size_t limit) const
{
    limit = std::min(limit == 0 ? MaxPage : limit, MaxPage);
    nlohmann::ordered_json items = nlohmann::ordered_json::array();
    std::size_t total = 0;
    for (QuestTemplateRow const& quest : _rows.Quests)
    {
        std::string const title = Text(quest.TitleKey);
        if (!Matches(search, { quest.Name, title }))
            continue;
        if (total++ < offset || items.size() >= limit)
            continue;
        std::size_t const goals = std::count_if(_rows.Goals.begin(), _rows.Goals.end(), [&quest](QuestGoalRow const& goal) { return goal.Quest == quest.Name; });
        items.push_back({ { "name", quest.Name }, { "title", title }, { "level", quest.Level }, { "mainline", quest.Mainline }, { "goals", goals },
            { "giver", quest.StarterTemplateId } });
    }
    return { { "total", total }, { "offset", offset }, { "items", std::move(items) } };
}

std::optional<nlohmann::ordered_json> StudioContent::Quest(std::string_view name) const
{
    auto const quest = std::find_if(_rows.Quests.begin(), _rows.Quests.end(), [name](QuestTemplateRow const& row) { return row.Name == name; });
    if (quest == _rows.Quests.end())
        return std::nullopt;
    auto const ofQuest = [name](auto const& row) { return row.Quest == name; };

    nlohmann::ordered_json goals = nlohmann::ordered_json::array();
    for (QuestGoalRow const* goal : RowsOf(_rows.Goals, ofQuest))
        goals.push_back({ { "name", goal->Goal }, { "type", goal->Type }, { "title", Text(goal->TitleKey) }, { "title_key", goal->TitleKey },
            { "destination_zone", goal->DestinationZone }, { "persona", goal->PersonaName }, { "tally", goal->TallyCount }, { "school", goal->School },
            { "requirements", Decode(goal->Requirements) } });

    nlohmann::ordered_json dialogs = nlohmann::ordered_json::array();
    for (QuestDialogEntryRow const* entry : RowsOf(_rows.DialogEntries, ofQuest))
        dialogs.push_back({ { "owner", entry->Owner }, { "goal", entry->Goal }, { "tag", entry->Tag }, { "position", entry->Position }, { "actor", entry->ActorTemplateId },
            { "text", Text(entry->DialogKey) }, { "key", entry->DialogKey }, { "event", entry->DialogEvent }, { "animation", entry->Animation } });

    nlohmann::ordered_json results = nlohmann::ordered_json::array();
    for (QuestResultRow const* result : RowsOf(_rows.Results, ofQuest))
    {
        nlohmann::ordered_json data = Decode(result->Data);
        nlohmann::ordered_json cinematics = NamedCinematics(data);
        results.push_back({ { "goal", result->Goal }, { "position", result->Position }, { "class", result->Class }, { "on_start", result->Start },
            { "data", std::move(data) }, { "cinematics", std::move(cinematics) } });
    }

    nlohmann::ordered_json prequests = nlohmann::ordered_json::array();
    nlohmann::ordered_json followups = nlohmann::ordered_json::array();
    std::string const complete = quest->Name;
    nlohmann::ordered_json const requirements = Decode(quest->Requirements);
    for (QuestTemplateRow const& other : _rows.Quests)
    {
        if (other.Name == quest->Name || other.Requirements.empty())
            continue;
        std::vector<std::string> strings;
        CollectStrings(Decode(other.Requirements), strings);
        if (std::find(strings.begin(), strings.end(), complete) != strings.end())
            followups.push_back(other.Name);
    }
    std::vector<std::string> strings;
    CollectStrings(requirements, strings);
    for (std::string const& text : strings)
        if (text != quest->Name && std::any_of(_rows.Quests.begin(), _rows.Quests.end(), [&text](QuestTemplateRow const& row) { return row.Name == text; }))
            prequests.push_back(text);

    return nlohmann::ordered_json{ { "name", quest->Name }, { "title", Text(quest->TitleKey) }, { "title_key", quest->TitleKey }, { "info", Text(quest->InfoKey) },
        { "level", quest->Level }, { "repeat", quest->Repeat }, { "mainline", quest->Mainline }, { "hidden", quest->IsHidden }, { "pet_only", quest->PetOnly },
        { "activity_type", quest->ActivityType }, { "giver", quest->StarterTemplateId }, { "requirement_list", quest->RequirementListId },
        { "requirements", requirements }, { "prequests", std::move(prequests) }, { "followups", std::move(followups) }, { "goals", std::move(goals) },
        { "dialogs", std::move(dialogs) }, { "results", std::move(results) } };
}

nlohmann::ordered_json StudioContent::Cinematics(std::string_view search, std::size_t offset, std::size_t limit) const
{
    limit = std::min(limit == 0 ? MaxPage : limit, MaxPage);
    nlohmann::ordered_json items = nlohmann::ordered_json::array();
    std::size_t total = 0;
    for (std::string const& name : _cinematics)
    {
        if (!Matches(search, { name }))
            continue;
        if (total++ < offset || items.size() >= limit)
            continue;
        auto const uses = _uses.find(name);
        items.push_back({ { "name", name }, { "used_by", uses == _uses.end() ? 0 : uses->second.size() } });
    }
    return { { "total", total }, { "offset", offset }, { "items", std::move(items) } };
}

std::optional<nlohmann::ordered_json> StudioContent::Cinematic(std::string_view name, std::string& error) const
{
    auto const known = _cinematicByKey.find(CinematicKey(name));
    if (!_root || known == _cinematicByKey.end())
        return std::nullopt;
    KiwadReadResult const read = _root->Read(known->second);
    if (!read.Succeeded())
    {
        error = read.Error;
        return std::nullopt;
    }
    nlohmann::ordered_json uses = nlohmann::ordered_json::array();
    if (auto const found = _uses.find(known->second); found != _uses.end())
        for (StudioCinematicUse const& use : found->second)
            uses.push_back({ { "quest", use.Quest }, { "goal", use.Goal }, { "class", use.Class }, { "on_start", use.Start } });
    std::string unread;
    nlohmann::ordered_json decoded = _catalog ? DecodeEntry(read.Data, unread) : nlohmann::ordered_json();
    return nlohmann::ordered_json{ { "name", known->second }, { "bytes", read.Data.size() }, { "used_by", std::move(uses) }, { "data", std::move(decoded) },
        { "unread", unread } };
}
