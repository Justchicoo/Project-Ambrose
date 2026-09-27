/*
 * Project Ambrose by Imjustchico
 * Opens every GameData archive in name order and reads the gamedata.bin of each that holds one as a versionable object, the form the client keeps zone data in. The decoder names every part of a class the dump does not list by its path, so an object list entry that came back empty is matched to the part it was and reported as a skipped object, a part deeper inside an entry is reported as a skipped part of a row that is still written, and any other problem the decoder names is an error, because it means the dump and the data disagree. A zone is known by its own m_zoneName, which is how the client is told where it is and how it finds the archive, so a name whose archive is not the one it was read from is an error rather than a second guess. Spawn requirements are kept as the versionable bytes the zone data holds them in, written again from the decoded object, so a stored client object stays in the client's own form.
 */

#include "ZoneExtractor.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "ObjectSerializer.h"
#include "ZoneViews.h"

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
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
    DecodeResult decoded = ObjectSerializer::Decode(catalog, data, ZoneDataOptions());
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
        if (!object)
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
        zone.Objects.push_back(std::move(*row));
    }
    extraction.Zones.push_back(std::move(zone));
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
        ReadZone(catalog, stem, data.Data, extraction);
    }
    if (progress)
        progress(archives.size(), archives.size());
    extraction.FinishErrors();
    return extraction;
}

std::optional<ZoneExtraction> ZoneExtractor::ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error,
    ZoneExtractionProgress const& progress)
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
    if (!registry.LoadFromFile(typeDump))
    {
        std::vector<std::string> const problems = registry.GetErrors();
        error = fmt::format("cannot load the type dump {}{}{}", ConfigMgr::PathToUtf8(typeDump), problems.empty() ? "" : ": ", problems.empty() ? std::string() : problems.front());
        return std::nullopt;
    }
    return Extract(gameData, registry.GetCatalog(), progress);
}
