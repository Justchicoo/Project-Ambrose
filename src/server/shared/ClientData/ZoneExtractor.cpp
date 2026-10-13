/*
 * Project Ambrose by Imjustchico
 * Opens every GameData archive in name order and reads the gamedata.bin of each that holds one as a versionable object, the form the client keeps zone data in. The decoder names every part of a class the dump does not list by its path, so an object list entry that came back empty is matched to the part it was and reported as a skipped object, while an entry of one of the sigil classes the dump does not list is read as the CoreObjectInfo it derives from, its inherited fields filled by their hashes and its own left out, and written with its own class name, which its hash proves, a part deeper inside an entry is reported as a skipped part of a row that is still written, and any other problem the decoder names is an error, because it means the dump and the data disagree. A zone is known by its own m_zoneName, which is how the client is told where it is and how it finds the archive, so a name whose archive is not the one it was read from is an error rather than a second guess. Spawn requirements are kept as the versionable bytes the zone data holds them in, written again from the decoded object, so a stored client object stays in the client's own form. A zone's volumes.xml and triggers.xml are BINd files whose classes the dump does not list, so each field is read by its name, and by its hash where no name fits yet, and a field that is missing or of another type fails the file rather than giving a row a default, where a null pointer is a value and a list entry of a class nothing describes, or any issue other than an unknown class, fails it too; a result of a class nothing describes keeps its place with the hash the file gives it, and a part deeper inside a kept entry is reported as a skipped part as in gamedata.bin. A zone's spawnData.xml is a BINd of classes the dump does list, so it is read through the spawn views, each spawner's requirements and each item's spawn requirements kept as versionable bytes the same way, and an item that is not a SpawnItem placing a SpawnObjectInfo fails the file.
 */

#include "ZoneExtractor.h"
#include "BindFile.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "ObjectSerializer.h"
#include "StringHash.h"
#include "ZoneViews.h"

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>

namespace
{
    constexpr std::string_view ObjectListMarker = ".m_objectList[";

    SerializerOptions ZoneDataOptions()
    {
        SerializerOptions options;
        options.Versionable = true;
        options.Flags = SerializerFlag::None;
        options.Mask = 0;
        options.AllowNullRoot = false;
        options.AllowTrailingBytes = false;
        return options;
    }

    struct EntryPart
    {
        std::size_t Index = 0;
        bool WholeEntry = false;
    };

    std::optional<EntryPart> ObjectListPart(std::string_view path)
    {
        std::size_t const marker = path.find(ObjectListMarker);
        if (marker == std::string_view::npos)
            return std::nullopt;
        std::size_t const first = marker + ObjectListMarker.size();
        std::size_t const close = path.find(']', first);
        if (close == std::string_view::npos)
            return std::nullopt;
        EntryPart part;
        auto const [end, error] = std::from_chars(path.data() + first, path.data() + close, part.Index);
        if (error != std::errc() || end != path.data() + close)
            return std::nullopt;
        part.WholeEntry = close + 1 == path.size();
        return part;
    }

    bool IsUnknownClassIssue(DecodeIssueKind kind) noexcept
    {
        return kind == DecodeIssueKind::UnknownClass || kind == DecodeIssueKind::UnknownClassProperty;
    }

    constexpr std::string_view SigilClasses[] = { "class CombatSigilInfo", "class MinigameSigilInfo", "class ConfigurableMinigameSigilInfo", "class PvPCombatSigilInfo",
        "class BattlegroundSigilInfo", "class DynamicSigilInfo" };

    std::optional<std::string> SigilClassName(uint32 hash)
    {
        auto const found = std::find_if(std::begin(SigilClasses), std::end(SigilClasses), [hash](std::string_view name) { return StringHash::KiStringHash(name) == hash; });
        if (found == std::end(SigilClasses))
            return std::nullopt;
        return std::string(*found);
    }

    std::optional<ExtractedObject> ObjectValues(CoreObjectInfoView const& info, std::string& error)
    {
        ExtractedObject row;
        row.ClassName = info.Target().GetClass().Name;
        row.TemplateId = info.GetTemplateId();
        row.ObjectId = info.GetObjectId();
        row.Location = info.GetLocation();
        row.Orientation = info.GetOrientation();
        row.Scale = info.GetScale();
        row.ZoneTag = info.GetZoneTag();
        row.StartState = info.GetStartState();
        row.OverrideName = info.GetOverrideName();
        row.GlobalDynamic = info.IsGlobalDynamic();
        row.Undetectable = info.IsUndetectable();
        if (PropertyObject const* const requirements = info.GetSpawnRequirements())
        {
            EncodeResult encoded = ObjectSerializer::Encode(requirements, ZoneDataOptions());
            if (!encoded.Ok())
            {
                error = encoded.Detail.empty() ? std::string(ObjectSerializer::GetStatusName(encoded.Status)) : std::move(encoded.Detail);
                return std::nullopt;
            }
            row.SpawnRequirements = std::move(encoded.Bytes);
        }
        row.LoadingType = info.GetLoadingType();
        return row;
    }

    std::optional<std::vector<uint8>> Versionable(PropertyObject const* object, std::string& error)
    {
        EncodeResult encoded = ObjectSerializer::Encode(object, ZoneDataOptions());
        if (!encoded.Ok())
        {
            error = encoded.Detail.empty() ? std::string(ObjectSerializer::GetStatusName(encoded.Status)) : std::move(encoded.Detail);
            return std::nullopt;
        }
        return std::move(encoded.Bytes);
    }

    class FieldReader
    {
    public:
        explicit FieldReader(PropertyObject const& object) : _object(object) { }

        std::string const& GetMissing() const noexcept { return _missing; }
        void Fail(std::string reason)
        {
            if (_missing.empty())
                _missing = std::move(reason);
        }

        std::string Text(std::string_view name) { return Take<std::string>(name, _object.Get(name)); }
        std::string Text(uint32 hash) { return Take<std::string>(fmt::format("#{}", hash), _object.Get(hash)); }
        bool Flag(std::string_view name) { return Take<bool>(name, _object.Get(name)); }
        bool Flag(uint32 hash) { return Take<bool>(fmt::format("#{}", hash), _object.Get(hash)); }
        float Real(std::string_view name) { return Take<float>(name, _object.Get(name)); }
        int64 Whole(std::string_view name) { return Whole(name, _object.Get(name)); }
        int64 Whole(uint32 hash) { return Whole(fmt::format("#{}", hash), _object.Get(hash)); }

        std::vector<std::string> Texts(std::string_view name)
        {
            std::vector<std::string> texts;
            PropertyValue const* const value = _object.Get(name);
            PropertyValue::List const* const list = value ? value->GetList() : nullptr;
            if (!list)
            {
                Miss(name);
                return texts;
            }
            for (PropertyValue const& entry : *list)
                texts.push_back(Take<std::string>(name, &entry));
            return texts;
        }

        PropertyTypes::Vector3D Point(std::string_view name) { return Take<PropertyTypes::Vector3D>(name, _object.Get(name)); }

        std::vector<uint64> Ids(std::string_view name)
        {
            std::vector<uint64> ids;
            PropertyValue const* const value = _object.Get(name);
            PropertyValue::List const* const list = value ? value->GetList() : nullptr;
            if (!list)
            {
                Miss(name);
                return ids;
            }
            for (PropertyValue const& entry : *list)
                ids.push_back(static_cast<uint64>(Whole(name, &entry)));
            return ids;
        }

        PropertyObject const* Object(std::string_view name)
        {
            PropertyValue const* const value = _object.Get(name);
            if (!value || (!value->IsNullObject() && !value->AsObject()))
            {
                Miss(name);
                return nullptr;
            }
            return value->AsObject();
        }

    private:
        void Miss(std::string_view name)
        {
            Fail(fmt::format("{} has no {} the rows can read", _object.GetClass().Name, name));
        }

        template<typename T>
        T Take(std::string_view name, PropertyValue const* value)
        {
            T const* const held = value ? value->GetIf<T>() : nullptr;
            if (!held)
            {
                Miss(name);
                return T{};
            }
            return *held;
        }

        int64 Whole(std::string_view name, PropertyValue const* value)
        {
            std::optional<int64> number;
            if (value)
                number = WholeOf(*value);
            if (!number)
                Miss(name);
            return number.value_or(0);
        }

        static std::optional<int64> WholeOf(PropertyValue const& value)
        {
            if (auto const* held = value.GetIf<int8>()) return *held;
            if (auto const* held = value.GetIf<uint8>()) return *held;
            if (auto const* held = value.GetIf<int16>()) return *held;
            if (auto const* held = value.GetIf<uint16>()) return *held;
            if (auto const* held = value.GetIf<int32>()) return *held;
            if (auto const* held = value.GetIf<uint32>()) return *held;
            if (auto const* held = value.GetIf<int64>()) return *held;
            if (auto const* held = value.GetIf<uint64>()) return static_cast<int64>(*held);
            return std::nullopt;
        }

        PropertyObject const& _object;
        std::string _missing;
    };

    struct FileIssues
    {
        std::map<std::string, uint32, std::less<>> UnknownAt;
        std::set<std::string, std::less<>> Placed;
        std::string Failure;
    };

    FileIssues SortIssues(std::vector<DecodeIssue> const& issues, std::string_view listPath)
    {
        FileIssues sorted;
        for (DecodeIssue const& issue : issues)
        {
            if (!IsUnknownClassIssue(issue.Kind))
            {
                if (sorted.Failure.empty())
                    sorted.Failure = fmt::format("{} at {}: {}", ObjectSerializer::GetIssueName(issue.Kind), issue.Path, issue.Detail);
                continue;
            }
            if (issue.Kind != DecodeIssueKind::UnknownClass)
                continue;
            std::string_view const path = issue.Path;
            std::size_t const close = path.starts_with(listPath) ? path.find(']', listPath.size()) : std::string_view::npos;
            if (close == std::string_view::npos || close + 1 == path.size())
            {
                if (sorted.Failure.empty())
                    sorted.Failure = fmt::format("{} is of class hash {}, which no class the server knows describes", issue.Path, issue.Hash);
                continue;
            }
            sorted.UnknownAt.emplace(issue.Path, issue.Hash);
        }
        return sorted;
    }

    std::optional<BindReadResult> ReadServerFile(TypeCatalogPtr const& catalog, std::span<uint8 const> data, std::string_view rootClass, std::string& failure)
    {
        BindReadResult read = BindFile::Read(catalog, data);
        if (!read.Ok() || !read.Decoded.Object)
        {
            failure = read.Detail.empty() ? std::string(BindFile::GetStatusName(read.Status)) : read.Detail;
            return std::nullopt;
        }
        if (read.Decoded.Object->GetClass().Name != rootClass)
        {
            failure = fmt::format("its root is {}, not {}", read.Decoded.Object->GetClass().Name, rootClass);
            return std::nullopt;
        }
        return read;
    }

    std::vector<ExtractedTriggerResult> Results(PropertyObject const* list, std::string const& path, FileIssues& issues, FieldReader& fields)
    {
        std::vector<ExtractedTriggerResult> results;
        if (!list)
            return results;
        PropertyValue const* const entries = list->Get("m_results");
        if (!entries || !entries->GetList())
        {
            fields.Fail(fmt::format("{} has no m_results list", path));
            return results;
        }
        for (std::size_t index = 0; index < entries->GetList()->size(); ++index)
        {
            std::string const at = fmt::format("{}.m_results[{}]", path, index);
            PropertyObject const* const result = (*entries->GetList())[index].AsObject();
            ExtractedTriggerResult row;
            if (!result)
            {
                auto const unknown = issues.UnknownAt.find(at);
                if (unknown == issues.UnknownAt.end())
                {
                    fields.Fail(fmt::format("{} is null", at));
                    return results;
                }
                issues.Placed.insert(at);
                row.ClassHash = unknown->second;
                results.push_back(std::move(row));
                continue;
            }
            row.ClassHash = result->GetClass().Hash;
            row.ClassName = result->GetClass().Name;
            std::string error;
            row.Data = Versionable(result, error);
            if (!row.Data)
            {
                fields.Fail(fmt::format("{} does not encode: {}", at, error));
                return results;
            }
            results.push_back(std::move(row));
        }
        return results;
    }

    std::optional<std::vector<uint8>> Encoded(PropertyObject const* object, std::string_view name, FieldReader& fields)
    {
        if (!object)
            return std::nullopt;
        std::string error;
        std::optional<std::vector<uint8>> bytes = Versionable(object, error);
        if (!bytes)
            fields.Fail(fmt::format("{} does not encode: {}", name, error));
        return bytes;
    }

    void SkipUnplacedParts(FileIssues const& issues, std::string const& zone, ZoneExtraction& extraction)
    {
        for (auto const& [path, hash] : issues.UnknownAt)
            if (!issues.Placed.contains(path))
                extraction.Skipped.push_back({ zone, path, hash, false });
    }
}

void ZoneExtraction::AddError(std::string error)
{
    ++ErrorCount;
    if (ErrorCount <= MaxReportedErrors)
        Errors.push_back(std::move(error));
}

void ZoneExtraction::FinishErrors()
{
    if (ErrorCount > MaxReportedErrors && Errors.size() == MaxReportedErrors)
        Errors.push_back(fmt::format("and {} more problems", ErrorCount - MaxReportedErrors));
}

std::size_t ZoneExtraction::GetLocationCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedZone const& zone : Zones)
        count += zone.Locations.size();
    return count;
}

std::size_t ZoneExtraction::GetObjectCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedZone const& zone : Zones)
        count += zone.Objects.size();
    return count;
}

std::size_t ZoneExtraction::GetSkippedObjectCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Skipped.begin(), Skipped.end(), [](SkippedZonePart const& part) { return part.WholeObject; }));
}

std::size_t ZoneExtraction::GetVolumeCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedZone const& zone : Zones)
        count += zone.Volumes.size();
    return count;
}

std::size_t ZoneExtraction::GetTriggerCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedZone const& zone : Zones)
        count += zone.Triggers.size();
    return count;
}

std::size_t ZoneExtraction::GetSpawnerCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedZone const& zone : Zones)
        count += zone.Spawners.size();
    return count;
}

std::size_t ZoneExtraction::GetPathCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedZone const& zone : Zones)
        count += zone.Paths.size();
    return count;
}

std::size_t ZoneExtraction::GetTriggerFailureZoneCount() const
{
    std::set<std::string_view> zones;
    for (TriggerFileFailure const& failure : TriggerFailures)
        zones.insert(failure.Zone);
    return zones.size();
}

ExtractedZone const* ZoneExtraction::Find(std::string_view path) const noexcept
{
    auto const found = std::find_if(Zones.begin(), Zones.end(), [path](ExtractedZone const& zone) { return zone.Path == path; });
    return found == Zones.end() ? nullptr : &*found;
}

std::string ZoneExtractor::ArchiveStemOf(std::string_view zonePath)
{
    std::string stem(zonePath);
    std::replace(stem.begin(), stem.end(), '/', '-');
    return stem;
}

void ZoneExtractor::ReadZone(TypeCatalogPtr const& catalog, std::string_view archiveStem, std::span<uint8 const> data, ZoneExtraction& extraction)
{
    SerializerOptions options = ZoneDataOptions();
    options.ReadUnknownClassAs = catalog ? catalog->FindClass("class CoreObjectInfo") : nullptr;
    DecodeResult decoded = ObjectSerializer::Decode(catalog, data, options);
    if (!decoded.Ok() || !decoded.Object)
    {
        extraction.AddError(fmt::format("{}: {} does not decode: {}", archiveStem, DataEntry, decoded.Detail.empty() ? ObjectSerializer::GetStatusName(decoded.Status) : decoded.Detail));
        return;
    }
    std::optional<WizZoneDataView> const root = WizZoneDataView::From(decoded.Object.get());
    if (!root)
    {
        extraction.AddError(fmt::format("{}: {} holds {}, which the zone view does not read; the type dump must list class WizZoneData", archiveStem, DataEntry,
            decoded.Object->GetClass().Name));
        return;
    }
    std::string const& path = root->GetZoneName();
    if (ArchiveStemOf(path) != archiveStem)
    {
        extraction.AddError(fmt::format("{}: {} names its zone {}, whose archive would be {}.wad", archiveStem, DataEntry, path, ArchiveStemOf(path)));
        return;
    }
    if (extraction.Find(path))
    {
        extraction.AddError(fmt::format("{}: zone {} is held by two archives", archiveStem, path));
        return;
    }

    std::vector<std::optional<uint32>> skippedEntries(root->GetObjects().size());
    std::size_t const errorsBefore = extraction.ErrorCount;
    for (DecodeIssue const& issue : decoded.Issues)
    {
        std::optional<EntryPart> const part = ObjectListPart(issue.Path);
        bool const ownPropertyOfReadEntry = issue.Kind == DecodeIssueKind::UnknownProperty && part && part->WholeEntry && part->Index < skippedEntries.size()
            && skippedEntries[part->Index];
        if (ownPropertyOfReadEntry)
            continue;
        if (!IsUnknownClassIssue(issue.Kind) || !part || part->Index >= skippedEntries.size())
        {
            extraction.AddError(fmt::format("{}: {} at {}: {}", path, ObjectSerializer::GetIssueName(issue.Kind), issue.Path, issue.Detail));
            continue;
        }
        if (issue.Kind != DecodeIssueKind::UnknownClass)
            continue;
        if (part->WholeEntry)
            skippedEntries[part->Index] = issue.Hash;
        else
            extraction.Skipped.push_back({ path, issue.Path, issue.Hash, false });
    }
    if (extraction.ErrorCount != errorsBefore)
        return;

    ExtractedZone zone;
    zone.Path = path;
    zone.DisplayNameKey = root->GetDisplayName();
    zone.FarClip = root->GetFarClip();
    zone.HealingPerMinute = root->GetHealingPerMinute();
    zone.SoftLimit = root->GetSoftLimit();
    zone.HardLimit = root->GetHardLimit();
    zone.NoMounts = root->HasNoMounts();

    for (PropertyValue const& value : root->GetLocations())
    {
        std::optional<LocationTemplateView> const location = LocationTemplateView::From(value.AsObject());
        if (!location)
        {
            extraction.AddError(fmt::format("{}: a location list entry is not a LocationTemplate", path));
            continue;
        }
        zone.Locations.push_back({ location->GetName(), location->GetLocation(), location->GetDirection() });
    }

    PropertyValue::List const& objects = root->GetObjects();
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        PropertyObject const* const object = objects[index].AsObject();
        std::optional<std::string> const sigil = skippedEntries[index] ? SigilClassName(*skippedEntries[index]) : std::nullopt;
        if (!object || (skippedEntries[index] && !sigil))
        {
            extraction.Skipped.push_back({ path, fmt::format("class WizZoneData.m_objectList[{}]", index), skippedEntries[index].value_or(0), true });
            continue;
        }
        std::optional<CoreObjectInfoView> const info = CoreObjectInfoView::From(object);
        if (!info)
        {
            extraction.AddError(fmt::format("{}: object list entry {} is {}, which is not a CoreObjectInfo", path, index, object->GetClass().Name));
            continue;
        }
        std::string error;
        std::optional<ExtractedObject> row = ObjectValues(*info, error);
        if (!row)
        {
            extraction.AddError(fmt::format("{}: the spawn requirements of object list entry {} do not encode: {}", path, index, error));
            continue;
        }
        if (sigil)
            row->ClassName = *sigil;
        zone.Objects.push_back(std::move(*row));
    }
    extraction.Zones.push_back(std::move(zone));
}

void ZoneExtractor::ReadVolumes(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> data, ZoneExtraction& extraction)
{
    constexpr std::string_view Root = "class TriggerVolumeList";
    std::string failure;
    std::optional<BindReadResult> const read = ReadServerFile(catalog, data, Root, failure);
    if (!read)
    {
        extraction.TriggerFailures.push_back({ zone.Path, std::string(VolumeEntry), std::move(failure) });
        return;
    }
    std::string const listPath = fmt::format("{}.m_allVolumes[", Root);
    FileIssues issues = SortIssues(read->Decoded.Issues, listPath);
    std::vector<ExtractedVolume> volumes;
    FieldReader root(*read->Decoded.Object);
    PropertyValue const* const list = read->Decoded.Object->Get("m_allVolumes");
    if (!list || !list->GetList())
        root.Fail("class TriggerVolumeList has no m_allVolumes list");
    for (std::size_t index = 0; issues.Failure.empty() && root.GetMissing().empty() && list && list->GetList() && index < list->GetList()->size(); ++index)
    {
        PropertyObject const* const entry = (*list->GetList())[index].AsObject();
        if (!entry)
        {
            root.Fail(fmt::format("{}{}] is null", listPath, index));
            break;
        }
        FieldReader fields(*entry);
        ExtractedVolume volume;
        volume.Name = fields.Text("m_triggerObjName");
        volume.ObjectId = static_cast<uint32>(fields.Whole("m_nObjectID"));
        volume.TemplateId = static_cast<uint64>(fields.Whole("m_templateID"));
        volume.Shape = fields.Text("m_shape");
        volume.Position = { fields.Real("m_locationX"), fields.Real("m_locationY"), fields.Real("m_locationZ") };
        volume.Radius = fields.Real("m_radius");
        volume.Length = fields.Real("m_length");
        volume.Width = fields.Real("m_width");
        volume.Depth = fields.Real("m_depth");
        volume.QuestEvents = fields.Flag("m_questEvents");
        volume.PlayerOnly = fields.Flag("m_playerOnly");
        volume.LoadingType = fields.Whole("m_loadingType");
        volume.SpawnRequirements = Encoded(fields.Object("m_spawnRequirements"), "m_spawnRequirements", fields);
        volume.EnterEvents = fields.Texts("m_enterEvents");
        volume.ExitEvents = fields.Texts("m_exitEvents");
        if (!fields.GetMissing().empty())
        {
            root.Fail(fmt::format("{}{}]: {}", listPath, index, fields.GetMissing()));
            break;
        }
        volumes.push_back(std::move(volume));
    }
    if (!issues.Failure.empty() || !root.GetMissing().empty())
    {
        extraction.TriggerFailures.push_back({ zone.Path, std::string(VolumeEntry), issues.Failure.empty() ? root.GetMissing() : issues.Failure });
        return;
    }
    SkipUnplacedParts(issues, zone.Path, extraction);
    zone.Volumes = std::move(volumes);
}

void ZoneExtractor::ReadTriggers(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> data, ZoneExtraction& extraction)
{
    constexpr std::string_view Root = "class TriggerList";
    std::string failure;
    std::optional<BindReadResult> const read = ReadServerFile(catalog, data, Root, failure);
    if (!read)
    {
        extraction.TriggerFailures.push_back({ zone.Path, std::string(TriggerEntry), std::move(failure) });
        return;
    }
    std::string const listPath = fmt::format("{}.m_allTriggers[", Root);
    FileIssues issues = SortIssues(read->Decoded.Issues, listPath);
    std::vector<ExtractedTrigger> triggers;
    FieldReader root(*read->Decoded.Object);
    PropertyValue const* const list = read->Decoded.Object->Get("m_allTriggers");
    if (!list || !list->GetList())
        root.Fail("class TriggerList has no m_allTriggers list");
    for (std::size_t index = 0; issues.Failure.empty() && root.GetMissing().empty() && list && list->GetList() && index < list->GetList()->size(); ++index)
    {
        PropertyObject const* const entry = (*list->GetList())[index].AsObject();
        if (!entry)
        {
            root.Fail(fmt::format("{}{}] is null", listPath, index));
            break;
        }
        std::string const at = fmt::format("{}{}]", listPath, index);
        FieldReader fields(*entry);
        ExtractedTrigger trigger;
        trigger.Name = fields.Text("m_triggerName");
        trigger.ClassName = entry->GetClass().Name;
        trigger.TriggerMax = static_cast<int32>(fields.Whole("m_triggerMax"));
        trigger.Cooldown = fields.Real("m_cooldown");
        trigger.Unnamed780900737 = static_cast<int32>(fields.Whole(780900737u));
        trigger.Unnamed847435658 = fields.Flag(847435658u);
        trigger.Unnamed1549045087 = fields.Text(1549045087u);
        trigger.Unnamed2293879431 = fields.Text(2293879431u);
        trigger.Requirements = Encoded(fields.Object("m_requirements"), "m_requirements", fields);
        trigger.ObjectInfo = Encoded(fields.Object("m_triggerObjectInfo"), "m_triggerObjectInfo", fields);
        trigger.ActivateEvents = fields.Texts("m_activateEvents");
        trigger.FireEvents = fields.Texts("m_fireEvents");
        trigger.DeactivateEvents = fields.Texts("m_deactivateEvents");
        trigger.Results = Results(fields.Object("m_results"), at + ".m_results", issues, fields);
        trigger.CooldownResults = Results(fields.Object("m_cooldownResults"), at + ".m_cooldownResults", issues, fields);
        if (trigger.ClassName == "class StateTrigger")
        {
            ExtractedStateTrigger& state = trigger.State.emplace();
            state.QuestEvent = fields.Text("m_questEvent");
            state.RequiredQuest = fields.Text("m_requiredQuest");
            state.RequiredState = fields.Text("m_requiredState");
            state.Unnamed333662217 = fields.Flag(333662217u);
            state.Unnamed758563334 = fields.Flag(758563334u);
            state.Unnamed1521843245 = fields.Texts("#1521843245");
            state.Unnamed3350245995 = fields.Text(3350245995u);
            state.Unnamed3431571632 = fields.Text(3431571632u);
        }
        else if (trigger.ClassName != "class Trigger")
            fields.Fail(fmt::format("{} is a {}, which the zone rows do not read", at, trigger.ClassName));
        if (!fields.GetMissing().empty())
        {
            root.Fail(fmt::format("{}: {}", at, fields.GetMissing()));
            break;
        }
        triggers.push_back(std::move(trigger));
    }
    if (!issues.Failure.empty() || !root.GetMissing().empty())
    {
        extraction.TriggerFailures.push_back({ zone.Path, std::string(TriggerEntry), issues.Failure.empty() ? root.GetMissing() : issues.Failure });
        return;
    }
    SkipUnplacedParts(issues, zone.Path, extraction);
    zone.Triggers = std::move(triggers);
}

void ZoneExtractor::ReadSpawns(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> data, ZoneExtraction& extraction)
{
    constexpr std::string_view Root = "class SpawnManager";
    std::string failure;
    std::optional<BindReadResult> const read = ReadServerFile(catalog, data, Root, failure);
    if (!read)
    {
        extraction.TriggerFailures.push_back({ zone.Path, std::string(SpawnEntry), std::move(failure) });
        return;
    }
    std::string const listPath = fmt::format("{}.m_spawners[", Root);
    FileIssues issues = SortIssues(read->Decoded.Issues, listPath);
    std::optional<SpawnManagerView> const root = SpawnManagerView::From(read->Decoded.Object.get());
    if (!root)
        failure = "class SpawnManager does not read through the spawn view; the type dump must list it";
    std::vector<ExtractedSpawner> spawners;
    PropertyValue::List const* const list = root ? &root->GetSpawners() : nullptr;
    for (std::size_t index = 0; issues.Failure.empty() && failure.empty() && list && index < list->size(); ++index)
    {
        std::string const at = fmt::format("{}{}]", listPath, index);
        std::optional<SpawnObjectView> const spawner = SpawnObjectView::From((*list)[index].AsObject());
        if (!spawner)
        {
            failure = fmt::format("{} is not a SpawnObject", at);
            break;
        }
        ExtractedSpawner row;
        row.Name = spawner->GetName();
        row.Id = spawner->GetId();
        row.Active = spawner->IsActive();
        row.PopSensitive = spawner->IsPopSensitive();
        row.MaxSpawns = spawner->GetMaxSpawns();
        row.AtLeastOneSpawn = spawner->HasAtLeastOneSpawn();
        row.ActivateAtMax = spawner->ActivatesAtMax();
        row.SpawnTime = spawner->GetSpawnTime();
        row.RespawnRate = spawner->GetRespawnRate();
        row.GlobalDynamic = spawner->IsGlobalDynamic();
        row.WaitForTimer = spawner->WaitsForTimer();
        row.ZoneLevelMin = spawner->GetZoneLevelMin();
        row.ZoneLevelMax = spawner->GetZoneLevelMax();
        row.ZoneLevelUp = spawner->GetZoneLevelUp();
        if (PropertyObject const* const requirements = spawner->GetGlobalDynamicReqs())
        {
            std::string error;
            row.GlobalDynamicReqs = Versionable(requirements, error);
            if (!row.GlobalDynamicReqs)
            {
                failure = fmt::format("{}.m_globalDynamicReqs does not encode: {}", at, error);
                break;
            }
        }
        PropertyValue::List const& items = spawner->GetSpawnList();
        for (std::size_t position = 0; failure.empty() && position < items.size(); ++position)
        {
            std::string const itemAt = fmt::format("{}.m_spawnList[{}]", at, position);
            std::optional<SpawnItemView> const item = SpawnItemView::From(items[position].AsObject());
            PropertyObject const* const placed = item ? item->GetObjectInfo() : nullptr;
            std::optional<CoreObjectInfoView> const info = CoreObjectInfoView::From(placed);
            std::optional<SpawnObjectInfoView> const spawnInfo = SpawnObjectInfoView::From(placed);
            if (!item || !info || !spawnInfo)
            {
                failure = fmt::format("{} is not a SpawnItem placing a SpawnObjectInfo", itemAt);
                break;
            }
            std::string error;
            std::optional<ExtractedObject> object = ObjectValues(*info, error);
            if (!object)
            {
                failure = fmt::format("the spawn requirements of {} do not encode: {}", itemAt, error);
                break;
            }
            row.Items.push_back({ item->GetPercentChance(), std::move(*object), spawnInfo->GetStartNodeType(), spawnInfo->GetStartNode(), spawnInfo->GetPathId(),
                spawnInfo->GetUniqueLoc() });
        }
        spawners.push_back(std::move(row));
    }
    if (!issues.Failure.empty() || !failure.empty())
    {
        extraction.TriggerFailures.push_back({ zone.Path, std::string(SpawnEntry), issues.Failure.empty() ? failure : issues.Failure });
        return;
    }
    SkipUnplacedParts(issues, zone.Path, extraction);
    zone.Spawners = std::move(spawners);
}

void ZoneExtractor::ReadPaths(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> paths, std::span<uint8 const> nodes, ZoneExtraction& extraction)
{
    constexpr std::string_view PathRoot = "class PathManager::PathTemplateList";
    constexpr std::string_view NodeRoot = "class PathManager::NodeTemplateList";
    auto const fail = [&](std::string_view file, std::string detail) { extraction.TriggerFailures.push_back({ zone.Path, std::string(file), std::move(detail) }); };
    DecodeResult const decodedNodes = ObjectSerializer::Decode(catalog, nodes, ZoneDataOptions());
    if (!decodedNodes.Ok() || !decodedNodes.Object)
        return fail(PathNodeEntry, decodedNodes.Detail.empty() ? std::string(ObjectSerializer::GetStatusName(decodedNodes.Status)) : decodedNodes.Detail);
    if (decodedNodes.Object->GetClass().Name != NodeRoot)
        return fail(PathNodeEntry, fmt::format("its root is {}, not {}", decodedNodes.Object->GetClass().Name, NodeRoot));
    std::string const nodeListPath = fmt::format("{}.m_nodeList[", NodeRoot);
    FileIssues const nodeIssues = SortIssues(decodedNodes.Issues, nodeListPath);
    if (!nodeIssues.Failure.empty())
        return fail(PathNodeEntry, nodeIssues.Failure);
    std::map<uint64, ExtractedPathNode> byId;
    FieldReader nodeRoot(*decodedNodes.Object);
    PropertyValue const* const nodeList = decodedNodes.Object->Get("m_nodeList");
    if (!nodeList || !nodeList->GetList())
        return fail(PathNodeEntry, fmt::format("{} has no m_nodeList list", NodeRoot));
    for (std::size_t index = 0; index < nodeList->GetList()->size(); ++index)
    {
        PropertyObject const* const entry = (*nodeList->GetList())[index].AsObject();
        if (!entry)
            return fail(PathNodeEntry, fmt::format("{}{}] is null", nodeListPath, index));
        FieldReader fields(*entry);
        ExtractedPathNode node;
        node.Location = fields.Point("m_location");
        node.Radius = fields.Real("m_fRadius");
        node.Id = static_cast<uint64>(fields.Whole("m_id"));
        node.Direction = fields.Real("m_direction");
        node.Roll = fields.Real("m_roll");
        if (!fields.GetMissing().empty())
            return fail(PathNodeEntry, fmt::format("{}{}]: {}", nodeListPath, index, fields.GetMissing()));
        if (!byId.emplace(node.Id, node).second)
            return fail(PathNodeEntry, fmt::format("{}{}] repeats node id {}", nodeListPath, index, node.Id));
    }
    std::string failure;
    std::optional<BindReadResult> const read = ReadServerFile(catalog, paths, PathRoot, failure);
    if (!read)
        return fail(PathEntry, std::move(failure));
    std::string const pathListPath = fmt::format("{}.m_pathList[", PathRoot);
    FileIssues const pathIssues = SortIssues(read->Decoded.Issues, pathListPath);
    if (!pathIssues.Failure.empty())
        return fail(PathEntry, pathIssues.Failure);
    PropertyValue const* const pathList = read->Decoded.Object->Get("m_pathList");
    if (!pathList || !pathList->GetList())
        return fail(PathEntry, fmt::format("{} has no m_pathList list", PathRoot));
    std::vector<ExtractedPath> extracted;
    std::set<uint64> seen;
    for (std::size_t index = 0; index < pathList->GetList()->size(); ++index)
    {
        PropertyObject const* const entry = (*pathList->GetList())[index].AsObject();
        if (!entry)
            return fail(PathEntry, fmt::format("{}{}] is null", pathListPath, index));
        FieldReader fields(*entry);
        ExtractedPath path;
        path.Id = static_cast<uint64>(fields.Whole("m_id"));
        path.Name = fields.Text("m_name");
        std::vector<uint64> const ids = fields.Ids("m_nodeIDs");
        if (!fields.GetMissing().empty())
            return fail(PathEntry, fmt::format("{}{}]: {}", pathListPath, index, fields.GetMissing()));
        if (!seen.insert(path.Id).second)
            return fail(PathEntry, fmt::format("{}{}] repeats path id {}", pathListPath, index, path.Id));
        for (uint64 const id : ids)
        {
            auto const node = byId.find(id);
            if (node == byId.end())
                return fail(PathEntry, fmt::format("path {} ({}) names node {}, which {} does not hold", path.Id, path.Name, id, PathNodeEntry));
            path.Nodes.push_back(node->second);
        }
        extracted.push_back(std::move(path));
    }
    zone.Paths = std::move(extracted);
}

ZoneExtraction ZoneExtractor::Extract(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, ZoneExtractionProgress const& progress)
{
    ZoneExtraction extraction;
    std::vector<std::filesystem::path> archives;
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(gameData, error), end; !error && iterator != end; iterator.increment(error))
        if (iterator->is_regular_file() && iterator->path().extension() == ".wad")
            archives.push_back(iterator->path());
    if (error)
    {
        extraction.AddError(fmt::format("cannot list {}: {}", ConfigMgr::PathToUtf8(gameData), error.message()));
        extraction.FinishErrors();
        return extraction;
    }
    std::sort(archives.begin(), archives.end());
    for (std::size_t index = 0; index < archives.size(); ++index)
    {
        if (progress && index != 0)
            progress(index, archives.size());
        std::filesystem::path const& file = archives[index];
        std::string const stem = ConfigMgr::PathToUtf8(file.stem());
        std::string openError;
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(file, openError);
        if (!archive)
        {
            extraction.AddError(fmt::format("{}: cannot open: {}", stem, openError));
            continue;
        }
        ++extraction.Archives;
        if (!archive->Find(DataEntry))
            continue;
        KiwadReadResult const data = archive->Read(DataEntry, MaxEntryBytes);
        if (!data.Succeeded())
        {
            extraction.AddError(fmt::format("{}: {}: {}", stem, DataEntry, data.Error));
            continue;
        }
        std::size_t const zones = extraction.Zones.size();
        ReadZone(catalog, stem, data.Data, extraction);
        if (extraction.Zones.size() == zones)
            continue;
        ExtractedZone& zone = extraction.Zones.back();
        for (std::string_view const entry : { VolumeEntry, TriggerEntry, SpawnEntry })
        {
            if (!archive->Find(entry))
                continue;
            KiwadReadResult const read = archive->Read(entry, MaxEntryBytes);
            if (!read.Succeeded())
            {
                extraction.TriggerFailures.push_back({ zone.Path, std::string(entry), read.Error });
                continue;
            }
            if (entry == VolumeEntry)
                ReadVolumes(catalog, zone, read.Data, extraction);
            else if (entry == TriggerEntry)
                ReadTriggers(catalog, zone, read.Data, extraction);
            else
                ReadSpawns(catalog, zone, read.Data, extraction);
        }
        if (archive->Find(PathEntry))
        {
            KiwadReadResult const paths = archive->Read(PathEntry, MaxEntryBytes);
            KiwadReadResult const nodes = archive->Find(PathNodeEntry) ? archive->Read(PathNodeEntry, MaxEntryBytes) : KiwadReadResult{};
            if (!paths.Succeeded())
                extraction.TriggerFailures.push_back({ zone.Path, std::string(PathEntry), paths.Error });
            else if (!archive->Find(PathNodeEntry))
                extraction.TriggerFailures.push_back({ zone.Path, std::string(PathNodeEntry), "the archive holds pathData.xml but no pathNodeData.bin" });
            else if (!nodes.Succeeded())
                extraction.TriggerFailures.push_back({ zone.Path, std::string(PathNodeEntry), nodes.Error });
            else
                ReadPaths(catalog, zone, paths.Data, nodes.Data, extraction);
        }
    }
    if (progress)
        progress(archives.size(), archives.size());
    extraction.FinishErrors();
    return extraction;
}

std::optional<ZoneExtraction> ZoneExtractor::ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error,
    ZoneExtractionProgress const& progress, TypeDumpLoader::RawDump supplement)
{
    std::filesystem::path const gameData = clientDir / "Data" / "GameData";
    if (!std::filesystem::is_directory(gameData))
    {
        error = fmt::format("{} is not a folder", ConfigMgr::PathToUtf8(gameData));
        return std::nullopt;
    }
    TypedViewRegistry views;
    ZoneViews::RegisterAll(views);
    TypeRegistry registry(&views);
    std::vector<std::string> refused;
    if (!supplement.Classes.empty() && !registry.SetSupplement(std::move(supplement), "the server classes", refused))
    {
        error = fmt::format("the server classes cannot join the type dump{}{}", refused.empty() ? "" : ": ", refused.empty() ? std::string() : refused.front());
        return std::nullopt;
    }
    if (!registry.LoadFromFile(typeDump))
    {
        std::vector<std::string> const problems = registry.GetErrors();
        error = fmt::format("cannot load the type dump {}{}{}", ConfigMgr::PathToUtf8(typeDump), problems.empty() ? "" : ": ", problems.empty() ? std::string() : problems.front());
        return std::nullopt;
    }
    return Extract(gameData, registry.GetCatalog(), progress);
}
