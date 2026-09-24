/*
 * Project Ambrose by Imjustchico
 * zone_extractor enumerates GameData zone archives, decodes each gamedata.bin with the user's type dump, reports failures by zone, and optionally writes reproducible SQL rows for decoded zone metadata, locations and object placements without copying client data into the repository.
 */

#include "KiwadArchive.h"
#include "Environment.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "PropertyJson.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;

    constexpr std::string_view Usage = R"(Usage: zone_extractor [options]

Decodes gamedata.bin from every GameData/*.wad archive in the user's install.

Options:
  --client <dir>      the install holding Data/GameData (default: AMBROSE_CLIENT_DIR)
  --type-dump <file>  the type dump made from that install (default: AMBROSE_TYPE_DUMP_PATH)
  --sql <file>        write decoded zone rows to this SQL file
  --dry-run           decode and report everything without writing SQL
  --help              print this text

Exit status: 0 when every discovered gamedata.bin decodes, 1 when an input
cannot be read or decoded, and 2 on bad usage.
)";

    struct Arguments
    {
        std::optional<std::filesystem::path> Client;
        std::optional<std::filesystem::path> TypeDump;
        std::optional<std::filesystem::path> Sql;
        bool DryRun = false;
        bool Help = false;
    };

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments result;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            if (arg == "--help" || arg == "-h")
                result.Help = true;
            else if (arg == "--dry-run")
                result.DryRun = true;
            else if (arg == "--client" || arg == "--type-dump" || arg == "--sql")
            {
                if (index + 1 >= args.size() || args[index + 1].starts_with("--"))
                {
                    error = fmt::format("{} needs a value", arg);
                    return std::nullopt;
                }
                std::filesystem::path const value = LogConfig::Utf8Path(args[++index]);
                if (arg == "--client")
                    result.Client = value;
                else if (arg == "--type-dump")
                    result.TypeDump = value;
                else
                    result.Sql = value;
            }
            else
            {
                error = fmt::format("unknown option {}", arg);
                return std::nullopt;
            }
        }
        if (result.Help)
            return result;
        if (result.DryRun && result.Sql)
        {
            error = "--dry-run and --sql cannot be combined";
            return std::nullopt;
        }
        return result;
    }

    void FromEnvironment(std::optional<std::filesystem::path>& value, char const* name)
    {
        if (value)
            return;
        if (std::optional<std::string> const raw = Ambrose::GetEnv(name); raw && !raw->empty())
            value = LogConfig::Utf8Path(*raw);
    }

    std::string ZonePath(std::filesystem::path const& wad, std::filesystem::path const& gameData)
    {
        std::filesystem::path const relative = std::filesystem::relative(wad, gameData);
        std::string name = relative.stem().string();
        std::size_t const separator = name.find('-');
        if (separator != std::string::npos)
            name.replace(separator, 1, "/");
        return name;
    }

    std::string SqlText(std::string_view value)
    {
        std::string escaped;
        escaped.reserve(value.size() + 2);
        escaped.push_back('\'');
        for (char const character : value)
        {
            if (character == '\'')
                escaped += "''";
            else
                escaped.push_back(character);
        }
        escaped.push_back('\'');
        return escaped;
    }

    std::string SqlNumber(PropertyValue const* value)
    {
        if (!value)
            return "NULL";
        if (auto const number = value->GetIf<int32>())
            return fmt::format("{}", *number);
        if (auto const number = value->GetIf<uint32>())
            return fmt::format("{}", *number);
        if (auto const number = value->GetIf<int64>())
            return fmt::format("{}", *number);
        if (auto const number = value->GetIf<uint64>())
            return fmt::format("{}", *number);
        if (auto const number = value->GetIf<float>())
            return fmt::format("{:.9g}", *number);
        if (auto const number = value->GetIf<double>())
            return fmt::format("{:.17g}", *number);
        if (auto const flag = value->GetIf<bool>())
            return *flag ? "1" : "0";
        return "NULL";
    }

    PropertyValue const* Find(PropertyObject const& object, std::initializer_list<std::string_view> names)
    {
        for (std::string_view const name : names)
            if (PropertyValue const* value = object.Get(name))
                return value;
        return nullptr;
    }

    std::string SqlString(PropertyValue const* value)
    {
        if (!value)
            return "''";
        if (auto const text = value->GetIf<std::string>())
            return SqlText(*text);
        return "''";
    }

    std::string ObjectJson(PropertyValue const* value)
    {
        if (!value || !value->AsObject())
            return "NULL";
        return SqlText(PropertyJson::Dump(value->AsObject(), -1));
    }

    std::string ValueJson(PropertyValue const* value)
    {
        if (!value)
            return "NULL";
        if (auto const vector = value->GetIf<PropertyTypes::Vector3D>())
            return SqlText(fmt::format("[{:.9g},{:.9g},{:.9g}]", vector->X, vector->Y, vector->Z));
        if (auto const quaternion = value->GetIf<PropertyTypes::Quaternion>())
            return SqlText(fmt::format("[{:.9g},{:.9g},{:.9g},{:.9g}]", quaternion->X, quaternion->Y, quaternion->Z, quaternion->W));
        if (auto const euler = value->GetIf<PropertyTypes::Euler>())
            return SqlText(fmt::format("[{:.9g},{:.9g},{:.9g}]", euler->Pitch, euler->Yaw, euler->Roll));
        if (value->GetIf<int32>() || value->GetIf<uint32>() || value->GetIf<int64>() || value->GetIf<uint64>() ||
            value->GetIf<float>() || value->GetIf<double>() || value->GetIf<bool>())
            return SqlNumber(value);
        return ObjectJson(value);
    }

    bool NumberEquals(PropertyValue const* value, uint32 expected)
    {
        if (!value)
            return false;
        if (auto const number = value->GetIf<uint32>())
            return *number == expected;
        if (auto const number = value->GetIf<int32>())
            return *number >= 0 && static_cast<uint32>(*number) == expected;
        if (auto const number = value->GetIf<uint64>())
            return *number == expected;
        if (auto const number = value->GetIf<int64>())
            return *number >= 0 && static_cast<uint64>(*number) == expected;
        return false;
    }

    bool NestedNumberEquals(PropertyObject const& object, std::initializer_list<std::string_view> names, uint32 expected)
    {
        for (std::string_view const name : names)
        {
            PropertyValue const* value = object.Get(name);
            if (NumberEquals(value, expected))
                return true;
            if (NumberEquals(object.Get(fmt::format("{}.m_full", name)), expected))
                return true;
            if (value)
                if (PropertyObject const* nested = value->AsObject())
                    if (NumberEquals(Find(*nested, { "m_full", "m_value", "m_id" }), expected))
                        return true;
        }
        return false;
    }

    std::size_t CountTemplate(PropertyValue const* objectsValue, uint32 expected)
    {
        PropertyValue::List const* objects = objectsValue ? objectsValue->GetList() : nullptr;
        if (!objects)
            return 0;
        return static_cast<std::size_t>(std::count_if(objects->begin(), objects->end(), [expected](PropertyValue const& value)
        {
            PropertyObject const* object = value.AsObject();
            return object && NestedNumberEquals(*object, { "m_templateID", "m_templateId", "m_templateIDHash" }, expected);
        }));
    }

    std::vector<std::string> LocationNames(PropertyValue const* locationsValue)
    {
        std::vector<std::string> names;
        PropertyValue::List const* locations = locationsValue ? locationsValue->GetList() : nullptr;
        if (!locations)
            return names;
        for (PropertyValue const& value : *locations)
        {
            PropertyObject const* location = value.AsObject();
            PropertyValue const* name = location ? Find(*location, { "m_locName", "m_name", "m_locationName", "m_key" }) : nullptr;
            if (name)
                if (std::string const* text = name->GetIf<std::string>())
                    names.push_back(*text);
        }
        return names;
    }

    void WriteRows(std::ostream& output, std::string_view zone, PropertyObject const& root)
    {
        output << fmt::format("INSERT INTO zone_template (zone_path, display_name_key, far_clip, healing_per_minute, soft_limit, hard_limit, no_mounts) VALUES ({}, {}, {}, {}, {}, {}, {});\n",
            SqlText(zone),
            SqlString(Find(root, { "m_zoneDisplayName", "m_displayName", "m_displayNameKey", "m_name" })),
            SqlNumber(Find(root, { "m_farClip", "m_farClipDistance" })),
            SqlNumber(Find(root, { "m_healingPerMinute", "m_healRate" })),
            SqlNumber(Find(root, { "m_nSoftLimit", "m_softLimit", "m_softPlayerLimit" })),
            SqlNumber(Find(root, { "m_nHardLimit", "m_hardLimit", "m_hardPlayerLimit" })),
            SqlNumber(Find(root, { "m_noMounts", "m_mountsDisabled" })));

        PropertyValue const* locationsValue = Find(root, { "m_locationList", "m_locations", "m_locationTemplates" });
        PropertyValue::List const* locations = locationsValue ? locationsValue->GetList() : nullptr;
        if (locations)
            for (PropertyValue const& value : *locations)
            {
                PropertyObject const* location = value.AsObject();
                if (!location)
                    continue;
                output << fmt::format("INSERT INTO zone_location (zone_path, name, location, direction) VALUES ({}, {}, {}, {});\n",
                    SqlText(zone),
                    SqlString(Find(*location, { "m_locName", "m_name", "m_locationName", "m_key" })),
                    ValueJson(Find(*location, { "m_location", "m_position" })),
                    ValueJson(Find(*location, { "m_direction", "m_orientation" })));
            }

        PropertyValue const* objectsValue = Find(root, { "m_objectList", "m_objects" });
        PropertyValue::List const* objects = objectsValue ? objectsValue->GetList() : nullptr;
        if (!objects)
            return;
        for (PropertyValue const& value : *objects)
        {
            PropertyObject const* object = value.AsObject();
            if (!object)
                continue;
            output << fmt::format("INSERT INTO zone_object (zone_path, template_id, object_id, location, orientation, scale, zone_tag, start_state, loading_type, spawn_requirements) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {});\n",
                SqlText(zone),
                SqlNumber(Find(*object, { "m_templateID.m_full", "m_templateID", "m_templateId", "m_templateIDHash" })),
                SqlNumber(Find(*object, { "m_nObjectID", "m_objectID", "m_objectId", "nObjectID" })),
                ValueJson(Find(*object, { "m_location", "m_position" })),
                ValueJson(Find(*object, { "m_orientation", "m_direction" })),
                SqlNumber(Find(*object, { "m_fScale", "m_scale" })),
                SqlString(Find(*object, { "m_zoneTag" })),
                SqlNumber(Find(*object, { "m_startState", "m_initialState" })),
                SqlNumber(Find(*object, { "m_loadingType" })),
                ObjectJson(Find(*object, { "m_spawnRequirements", "m_spawnRequirement" })));
        }
    }

    int Run(Arguments arguments)
    {
        FromEnvironment(arguments.Client, "AMBROSE_CLIENT_DIR");
        FromEnvironment(arguments.TypeDump, "AMBROSE_TYPE_DUMP_PATH");
        if (!arguments.Client || !arguments.TypeDump)
        {
            std::cerr << "zone_extractor: --client and --type-dump are required, or set AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH\n";
            return Failure;
        }

        TypeRegistry registry;
        if (!registry.LoadFromFile(*arguments.TypeDump))
        {
            std::cerr << fmt::format("zone_extractor: cannot load type dump {}\n", arguments.TypeDump->string());
            for (std::string const& problem : registry.GetErrors())
                std::cerr << "  " << problem << '\n';
            return Failure;
        }

        std::filesystem::path const gameData = *arguments.Client / "Data" / "GameData";
        if (!std::filesystem::is_directory(gameData))
        {
            std::cerr << fmt::format("zone_extractor: GameData directory does not exist: {}\n", gameData.string());
            return Failure;
        }

        std::vector<std::filesystem::path> archives;
        for (std::filesystem::directory_iterator iterator(gameData); iterator != std::filesystem::directory_iterator(); ++iterator)
            if (iterator->is_regular_file() && iterator->path().extension() == ".wad")
                archives.push_back(iterator->path());
        std::sort(archives.begin(), archives.end());

        std::ofstream sql;
        if (arguments.Sql)
        {
            sql.open(*arguments.Sql, std::ios::binary | std::ios::trunc);
            if (!sql)
            {
                std::cerr << fmt::format("zone_extractor: cannot write {}\n", arguments.Sql->string());
                return Failure;
            }
        }

        TypeCatalogPtr const catalog = registry.GetCatalog();
        SerializerOptions options;
        options.Versionable = true;
        options.Flags = SerializerFlag::None;
        options.Mask = 0;
        options.AllowNullRoot = false;
        options.AllowTrailingBytes = false;
        std::size_t withData = 0;
        std::size_t decoded = 0;
        std::size_t failures = 0;

        for (std::filesystem::path const& path : archives)
        {
            std::string error;
            std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(path, error);
            if (!archive)
            {
                ++failures;
                std::cout << fmt::format("{}: open failed: {}\n", path.filename().string(), error);
                continue;
            }
            KiwadReadResult const data = archive->Read("gamedata.bin");
            if (!data.Succeeded())
                continue;
            ++withData;
            DecodeResult const result = ObjectSerializer::Decode(catalog, data.Data, options);
            std::string const zone = ZonePath(path, gameData);
            if (!result.Ok() || !result.Object)
            {
                ++failures;
                std::cout << fmt::format("{}: decode failed: {}\n", zone, result.Detail.empty() ? ObjectSerializer::GetStatusName(result.Status) : result.Detail);
                continue;
            }
            ++decoded;
            std::size_t objectCount = 0;
            if (PropertyValue const* objects = Find(*result.Object, { "m_objectList", "m_objects" }))
                if (PropertyValue::List const* list = objects->GetList())
                    objectCount = list->size();
            std::cout << fmt::format("{}: {} root, {} objects\n", zone, result.Object->GetClass().Name, objectCount);
            if (zone == "WizardCity/WC_Hub" || zone == "WizardCity/WC_Ravenwood")
            {
                std::vector<std::string> const names = LocationNames(Find(*result.Object, { "m_locationList", "m_locations", "m_locationTemplates" }));
                std::cout << fmt::format("{}: display={}, locations={}, templates 38232={}, 38230={}, 81102={}, 1451035={}, 39088={}\n",
                    zone,
                    SqlString(Find(*result.Object, { "m_zoneDisplayName", "m_displayName", "m_displayNameKey" })),
                    names.size(),
                    CountTemplate(Find(*result.Object, { "m_objectList", "m_objects" }), 38232),
                    CountTemplate(Find(*result.Object, { "m_objectList", "m_objects" }), 38230),
                    CountTemplate(Find(*result.Object, { "m_objectList", "m_objects" }), 81102),
                    CountTemplate(Find(*result.Object, { "m_objectList", "m_objects" }), 1451035),
                    CountTemplate(Find(*result.Object, { "m_objectList", "m_objects" }), 39088));
                for (std::string const& name : names)
                    if (name == "Start" || name.find("Target location") != std::string::npos)
                        std::cout << fmt::format("{}: location {}\n", zone, name);
            }
            if (sql)
                WriteRows(sql, zone, *result.Object);
        }

        std::cout << fmt::format("Scanned {} zone WADs: {} with gamedata.bin, {} decoded, {} failed\n", archives.size(), withData, decoded, failures);
        return failures == 0 ? Success : Failure;
    }
}

int main(int argc, char** argv)
{
    try
    {
        std::vector<std::string> args;
        for (int index = 0; index < argc; ++index)
            args.emplace_back(argv[index]);
        std::string error;
        std::optional<Arguments> parsed = Parse(args, error);
        if (!parsed)
        {
            std::cerr << "zone_extractor: " << error << "\n\n" << Usage;
            return BadUsage;
        }
        if (parsed->Help)
        {
            std::cout << Usage;
            return Success;
        }
        return Run(std::move(*parsed));
    }
    catch (std::exception const& error)
    {
        std::cerr << "zone_extractor: " << error.what() << '\n';
        return Failure;
    }
}
