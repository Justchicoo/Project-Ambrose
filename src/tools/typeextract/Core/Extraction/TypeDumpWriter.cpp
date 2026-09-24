/*
 * Project Ambrose by Imjustchico
 * Renders a dump model as format v2 JSON with the extraction metadata first, classes sorted by name then key and every property, option and duplicate kept in model order, saves text through a temporary file named for the process and a random number, created exclusively in the target's folder and renamed over the target, and pairs two models by class, property and option name to list each difference in presence, hash, bases, order, property fields and option values in name order.
 */

#include "TypeDumpWriter.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <chrono>
#include <thread>
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace
{
    using OrderedJson = nlohmann::ordered_json;
    using Option = std::pair<std::string, std::variant<int64, std::string>>;

    constexpr std::string_view Present = "present";
    constexpr std::string_view Missing = "missing";

    OrderedJson OptionsJson(std::vector<Option> const& options)
    {
        OrderedJson json = OrderedJson::object();
        OrderedJson::object_t& entries = json.get_ref<OrderedJson::object_t&>();
        entries.reserve(options.size());
        for (auto const& [name, value] : options)
            entries.emplace_back(name, std::visit([](auto const& alternative) { return OrderedJson(alternative); }, value));
        return json;
    }

    OrderedJson PropertyJson(TypeDumpLoader::RawProperty const& property)
    {
        OrderedJson json = OrderedJson::object();
        if (property.Type)
            json["type"] = *property.Type;
        if (property.Id)
            json["id"] = *property.Id;
        if (property.Offset)
            json["offset"] = *property.Offset;
        if (property.Flags)
            json["flags"] = *property.Flags;
        if (property.Container)
            json["container"] = *property.Container;
        if (property.Dynamic)
            json["dynamic"] = *property.Dynamic;
        if (property.Singleton)
            json["singleton"] = *property.Singleton;
        if (property.Pointer)
            json["pointer"] = *property.Pointer;
        if (property.Hash)
            json["hash"] = *property.Hash;
        if (!property.Options.empty())
            json["enum_options"] = OptionsJson(property.Options);
        return json;
    }

    OrderedJson ClassJson(TypeDumpLoader::RawClass const& rawClass)
    {
        OrderedJson json = OrderedJson::object();
        if (rawClass.Name)
            json["name"] = *rawClass.Name;
        json["bases"] = rawClass.Bases;
        if (rawClass.Hash)
            json["hash"] = *rawClass.Hash;
        OrderedJson properties = OrderedJson::object();
        OrderedJson::object_t& entries = properties.get_ref<OrderedJson::object_t&>();
        entries.reserve(rawClass.Properties.size());
        for (TypeDumpLoader::RawProperty const& property : rawClass.Properties)
            entries.emplace_back(property.Name, PropertyJson(property));
        json["properties"] = std::move(properties);
        return json;
    }

    std::string PathText(std::filesystem::path const& path)
    {
        try
        {
            return ConfigMgr::PathToUtf8(path);
        }
        catch (std::exception const&)
        {
            return "the output path";
        }
    }

    class TemporaryFile
    {
    public:
        explicit TemporaryFile(std::filesystem::path path) : _path(std::move(path))
        {
        }

        TemporaryFile(TemporaryFile const&) = delete;
        TemporaryFile& operator=(TemporaryFile const&) = delete;

        ~TemporaryFile()
        {
            std::string ignored;
            Close(ignored);
        }

#ifdef _WIN32
        bool Create(std::string& error)
        {
            _handle = ::CreateFileW(_path.c_str(), GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (_handle == INVALID_HANDLE_VALUE)
            {
                error = LastError();
                return false;
            }
            return true;
        }

        bool Finish(std::string&)
        {
            return true;
        }

        bool Publish(std::filesystem::path const& target, std::string& error)
        {
            std::wstring const name = std::filesystem::absolute(target).native();
            std::size_t const bytes = sizeof(FILE_RENAME_INFO) + name.size() * sizeof(wchar_t);
            std::vector<uint64> storage(bytes / sizeof(uint64) + 1, 0);
            FILE_RENAME_INFO* const info = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
            info->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
            info->RootDirectory = nullptr;
            info->FileNameLength = static_cast<DWORD>(name.size() * sizeof(wchar_t));
            std::copy(name.begin(), name.end(), info->FileName);
            DWORD const size = static_cast<DWORD>(storage.size() * sizeof(uint64));
            DWORD code = Rename(target, FileRenameInfoEx, info, size);
            if (code == ERROR_INVALID_PARAMETER || code == ERROR_NOT_SUPPORTED)
            {
                info->Flags = 0;
                info->ReplaceIfExists = TRUE;
                code = Rename(target, FileRenameInfo, info, size);
            }
            if (code != ERROR_SUCCESS)
            {
                error = Message(code);
                return false;
            }
            return Close(error);
        }

        bool Write(std::string_view bytes, std::string& error)
        {
            while (!bytes.empty())
            {
                DWORD const chunk = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), MaxChunk));
                DWORD written = 0;
                if (!::WriteFile(_handle, bytes.data(), chunk, &written, nullptr) || written == 0)
                {
                    error = LastError();
                    return false;
                }
                bytes.remove_prefix(written);
            }
            return true;
        }

        bool Close(std::string& error)
        {
            if (_handle == INVALID_HANDLE_VALUE)
                return true;
            if (!::CloseHandle(std::exchange(_handle, INVALID_HANDLE_VALUE)))
            {
                error = LastError();
                return false;
            }
            return true;
        }

        static uint64 ProcessId()
        {
            return ::GetCurrentProcessId();
        }

    private:
        static constexpr uint32 RenameAttempts = 50;

        DWORD Rename(std::filesystem::path const& target, FILE_INFO_BY_HANDLE_CLASS kind, FILE_RENAME_INFO* info, DWORD size)
        {
            for (uint32 attempt = 1;; ++attempt)
            {
                if (::SetFileInformationByHandle(_handle, kind, info, size))
                    return ERROR_SUCCESS;
                DWORD const code = ::GetLastError();
                bool const transient = code == ERROR_ACCESS_DENIED || code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION;
                std::error_code ignored;
                if (!transient || attempt >= RenameAttempts || std::filesystem::is_directory(target, ignored))
                    return code;
                std::this_thread::sleep_for(std::chrono::milliseconds(attempt < 10 ? 1 : 10));
            }
        }

        static std::string Message(DWORD code)
        {
            return std::system_category().message(static_cast<int>(code));
        }

        static std::string LastError()
        {
            return Message(::GetLastError());
        }

        HANDLE _handle = INVALID_HANDLE_VALUE;
#else
        bool Create(std::string& error)
        {
            do
                _descriptor = ::open(_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
            while (_descriptor < 0 && errno == EINTR);
            if (_descriptor < 0)
            {
                error = LastError();
                return false;
            }
            return true;
        }

        bool Finish(std::string& error)
        {
            return Close(error);
        }

        bool Publish(std::filesystem::path const& target, std::string& error)
        {
            std::error_code renamed;
            std::filesystem::rename(_path, target, renamed);
            if (renamed)
            {
                error = renamed.message();
                return false;
            }
            return true;
        }

        bool Write(std::string_view bytes, std::string& error)
        {
            while (!bytes.empty())
            {
                ssize_t const written = ::write(_descriptor, bytes.data(), std::min<std::size_t>(bytes.size(), MaxChunk));
                if (written < 0 && errno == EINTR)
                    continue;
                if (written <= 0)
                {
                    error = written < 0 ? LastError() : std::string("the file accepted no bytes");
                    return false;
                }
                bytes.remove_prefix(static_cast<std::size_t>(written));
            }
            return true;
        }

        bool Close(std::string& error)
        {
            if (_descriptor < 0)
                return true;
            if (::close(std::exchange(_descriptor, -1)) != 0 && errno != EINTR)
            {
                error = LastError();
                return false;
            }
            return true;
        }

        static uint64 ProcessId()
        {
            return static_cast<uint64>(::getpid());
        }

    private:
        static std::string LastError()
        {
            return std::generic_category().message(errno);
        }

        int _descriptor = -1;
#endif

        static constexpr std::size_t MaxChunk = 1u << 20;

        std::filesystem::path _path;
    };

    std::filesystem::path TemporaryPathFor(std::filesystem::path const& path)
    {
        std::random_device device;
        uint64 const random = (uint64{ device() } << 32) | device();
        std::filesystem::path temporary = path;
        temporary += fmt::format(".{}.{:016x}.partial", TemporaryFile::ProcessId(), random);
        return temporary;
    }

    void RemoveTemporary(std::filesystem::path const& temporary) noexcept
    {
        std::error_code error;
        std::filesystem::remove(temporary, error);
    }

    template<typename Identity>
    using OccurrenceKey = std::pair<Identity, std::size_t>;

    template<typename Item, typename Identity>
    struct Pairing
    {
        std::map<OccurrenceKey<Identity>, std::pair<Item const*, Item const*>> Entries;
        std::vector<OccurrenceKey<Identity>> OursOrder;
        std::vector<OccurrenceKey<Identity>> TheirsOrder;
    };

    template<typename Item>
    std::vector<Item const*> Pointers(std::vector<Item> const& items)
    {
        std::vector<Item const*> pointers;
        pointers.reserve(items.size());
        for (Item const& item : items)
            pointers.push_back(&item);
        return pointers;
    }

    std::vector<TypeDumpLoader::RawClass const*> SortedClasses(TypeDumpLoader::RawDump const& dump)
    {
        std::vector<TypeDumpLoader::RawClass const*> classes = Pointers(dump.Classes);
        std::stable_sort(classes.begin(), classes.end(), [](TypeDumpLoader::RawClass const* left, TypeDumpLoader::RawClass const* right)
        {
            return std::tie(left->Name, left->Key) < std::tie(right->Name, right->Key);
        });
        return classes;
    }

    template<typename Item, typename IdentityOf>
    Pairing<Item, std::invoke_result_t<IdentityOf, Item const&>> PairItems(std::vector<Item const*> const& ours, std::vector<Item const*> const& theirs, IdentityOf identityOf)
    {
        using Identity = std::invoke_result_t<IdentityOf, Item const&>;
        Pairing<Item, Identity> pairing;
        auto const collect = [&pairing, &identityOf](std::vector<Item const*> const& items, bool isOurs, std::vector<OccurrenceKey<Identity>>& order)
        {
            std::map<Identity, std::size_t> occurrences;
            order.reserve(items.size());
            for (Item const* item : items)
            {
                Identity identity = identityOf(*item);
                std::size_t const occurrence = occurrences[identity]++;
                OccurrenceKey<Identity> key{ std::move(identity), occurrence };
                std::pair<Item const*, Item const*>& entry = pairing.Entries[key];
                (isOurs ? entry.first : entry.second) = item;
                order.push_back(std::move(key));
            }
        };
        collect(ours, true, pairing.OursOrder);
        collect(theirs, false, pairing.TheirsOrder);
        auto const unmatched = [&pairing](OccurrenceKey<Identity> const& key)
        {
            std::pair<Item const*, Item const*> const entry = pairing.Entries.find(key)->second;
            return entry.first == nullptr || entry.second == nullptr;
        };
        std::erase_if(pairing.OursOrder, unmatched);
        std::erase_if(pairing.TheirsOrder, unmatched);
        return pairing;
    }

    std::pair<bool, std::string_view> ClassIdentity(TypeDumpLoader::RawClass const& rawClass)
    {
        if (rawClass.Name)
            return { false, *rawClass.Name };
        return { true, rawClass.Key };
    }

    std::string_view PropertyIdentity(TypeDumpLoader::RawProperty const& property)
    {
        return property.Name;
    }

    std::string_view OptionIdentity(Option const& option)
    {
        return option.first;
    }

    std::string ClassLabel(TypeDumpLoader::RawClass const& rawClass)
    {
        if (rawClass.Name)
            return *rawClass.Name;
        return fmt::format("the class under key {}", rawClass.Key);
    }

    std::string Text(std::optional<std::string> const& value)
    {
        return value ? *value : std::string(Missing);
    }

    std::string Text(std::optional<uint64> const& value)
    {
        return value ? fmt::format("{}", *value) : std::string(Missing);
    }

    std::string Text(std::optional<bool> const& value)
    {
        if (!value)
            return std::string(Missing);
        return *value ? "true" : "false";
    }

    std::string OptionText(std::variant<int64, std::string> const& value)
    {
        if (int64 const* const number = std::get_if<int64>(&value))
            return fmt::format("{}", *number);
        return fmt::format("\"{}\"", std::get<std::string>(value));
    }

    std::string List(std::vector<std::string> const& names)
    {
        std::string text = "[";
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            if (index != 0)
                text += ", ";
            text += names[index];
        }
        text += ']';
        return text;
    }

    std::string List(std::vector<OccurrenceKey<std::string_view>> const& keys)
    {
        std::string text = "[";
        for (std::size_t index = 0; index < keys.size(); ++index)
        {
            if (index != 0)
                text += ", ";
            text += keys[index].first;
        }
        text += ']';
        return text;
    }

    class DifferenceList
    {
    public:
        explicit DifferenceList(std::vector<TypeDumpDifference>& differences) : _differences(differences)
        {
        }

        void Add(std::string const& className, std::string_view property, std::string field, std::string ours, std::string theirs)
        {
            _differences.push_back(TypeDumpDifference{ className, std::string(property), std::move(field), std::move(ours), std::move(theirs) });
        }

        void AddPresence(std::string const& className, std::string_view property, std::string field, bool inOurs)
        {
            Add(className, property, std::move(field), std::string(inOurs ? Present : Missing), std::string(inOurs ? Missing : Present));
        }

        template<typename Value>
        void AddIfDifferent(std::string const& className, std::string_view property, std::string_view field, std::optional<Value> const& ours, std::optional<Value> const& theirs)
        {
            if (ours != theirs)
                Add(className, property, std::string(field), Text(ours), Text(theirs));
        }

    private:
        std::vector<TypeDumpDifference>& _differences;
    };

    void CompareOptions(DifferenceList& list, std::string const& className, TypeDumpLoader::RawProperty const& ours, TypeDumpLoader::RawProperty const& theirs)
    {
        Pairing<Option, std::string_view> const options = PairItems(Pointers(ours.Options), Pointers(theirs.Options), OptionIdentity);
        if (options.OursOrder != options.TheirsOrder)
            list.Add(className, ours.Name, "option order", List(options.OursOrder), List(options.TheirsOrder));
        for (auto const& [key, entry] : options.Entries)
        {
            std::string field = fmt::format("option {}", key.first);
            if (entry.first == nullptr || entry.second == nullptr)
            {
                list.AddPresence(className, ours.Name, std::move(field), entry.first != nullptr);
                continue;
            }
            if (entry.first->second != entry.second->second)
                list.Add(className, ours.Name, std::move(field), OptionText(entry.first->second), OptionText(entry.second->second));
        }
    }

    void CompareProperties(DifferenceList& list, std::string const& className, TypeDumpLoader::RawProperty const& ours, TypeDumpLoader::RawProperty const& theirs)
    {
        list.AddIfDifferent(className, ours.Name, "type", ours.Type, theirs.Type);
        list.AddIfDifferent(className, ours.Name, "id", ours.Id, theirs.Id);
        list.AddIfDifferent(className, ours.Name, "offset", ours.Offset, theirs.Offset);
        list.AddIfDifferent(className, ours.Name, "flags", ours.Flags, theirs.Flags);
        list.AddIfDifferent(className, ours.Name, "container", ours.Container, theirs.Container);
        list.AddIfDifferent(className, ours.Name, "dynamic", ours.Dynamic, theirs.Dynamic);
        list.AddIfDifferent(className, ours.Name, "singleton", ours.Singleton, theirs.Singleton);
        list.AddIfDifferent(className, ours.Name, "pointer", ours.Pointer, theirs.Pointer);
        list.AddIfDifferent(className, ours.Name, "hash", ours.Hash, theirs.Hash);
        CompareOptions(list, className, ours, theirs);
    }

    void CompareClasses(DifferenceList& list, std::string const& className, TypeDumpLoader::RawClass const& ours, TypeDumpLoader::RawClass const& theirs)
    {
        list.AddIfDifferent(className, {}, "hash", ours.Hash, theirs.Hash);
        if (ours.Bases != theirs.Bases)
            list.Add(className, {}, "bases", List(ours.Bases), List(theirs.Bases));
        Pairing<TypeDumpLoader::RawProperty, std::string_view> const properties = PairItems(Pointers(ours.Properties), Pointers(theirs.Properties), PropertyIdentity);
        if (properties.OursOrder != properties.TheirsOrder)
            list.Add(className, {}, "order", List(properties.OursOrder), List(properties.TheirsOrder));
        for (auto const& [key, entry] : properties.Entries)
        {
            if (entry.first == nullptr || entry.second == nullptr)
            {
                list.AddPresence(className, key.first, "property", entry.first != nullptr);
                continue;
            }
            CompareProperties(list, className, *entry.first, *entry.second);
        }
    }

    std::string Printable(std::string_view text)
    {
        return Ambrose::ForLog(text, std::numeric_limits<std::size_t>::max());
    }
}

std::string TypeDumpWriter::ToJson(TypeDumpLoader::RawDump const& dump, TypeDumpMetadata const& metadata)
{
    std::vector<TypeDumpLoader::RawClass const*> const classes = SortedClasses(dump);
    OrderedJson root = OrderedJson::object();
    root["version"] = TypeDumpLoader::SupportedVersion;
    root["revision"] = metadata.Revision;
    root["executable_sha256"] = metadata.ExecutableSha256;
    root["extractor"] = metadata.Extractor;
    OrderedJson classesJson = OrderedJson::object();
    OrderedJson::object_t& entries = classesJson.get_ref<OrderedJson::object_t&>();
    entries.reserve(classes.size());
    for (TypeDumpLoader::RawClass const* rawClass : classes)
        entries.emplace_back(rawClass->Key, ClassJson(*rawClass));
    root["classes"] = std::move(classesJson);

    std::string text = root.dump(1, ' ', false, OrderedJson::error_handler_t::replace);
    text += '\n';
    return text;
}

bool TypeDumpWriter::Save(std::filesystem::path const& path, std::string const& text, std::string& error)
{
    std::filesystem::path ownTemporary;
    try
    {
        if (!path.has_filename())
        {
            error = fmt::format("{}: the path names a folder, not a file", PathText(path));
            return false;
        }
        std::filesystem::path const parent = path.parent_path();
        if (!parent.empty())
        {
            std::error_code created;
            std::filesystem::create_directories(parent, created);
            if (created)
            {
                error = fmt::format("{}: cannot create its folder {}: {}", PathText(path), PathText(parent), created.message());
                return false;
            }
            std::error_code checked;
            if (!std::filesystem::is_directory(parent, checked))
            {
                error = fmt::format("{}: {} is not a folder: {}", PathText(path), PathText(parent), checked ? checked.message() : "a file of that name is in the way");
                return false;
            }
        }

        std::filesystem::path const temporary = TemporaryPathFor(path);
        std::string failure;
        TemporaryFile file(temporary);
        if (!file.Create(failure))
        {
            error = fmt::format("{}: cannot create the temporary file {}: {}", PathText(path), PathText(temporary), failure);
            return false;
        }
        ownTemporary = temporary;
        std::string ignored;
        if (!file.Write(text, failure) || !file.Finish(failure))
        {
            error = fmt::format("{}: cannot write the temporary file {}: {}", PathText(path), PathText(temporary), failure);
            file.Close(ignored);
            RemoveTemporary(temporary);
            return false;
        }
        if (!file.Publish(path, failure))
        {
            error = fmt::format("{}: cannot replace it with the temporary file {}: {}", PathText(path), PathText(temporary), failure);
            file.Close(ignored);
            RemoveTemporary(temporary);
            return false;
        }
        return true;
    }
    catch (std::exception const& exception)
    {
        error = fmt::format("{}: cannot save the file: {}", PathText(path), exception.what());
        if (!ownTemporary.empty())
            RemoveTemporary(ownTemporary);
        return false;
    }
}

std::vector<TypeDumpDifference> TypeDumpWriter::Compare(TypeDumpLoader::RawDump const& ours, TypeDumpLoader::RawDump const& theirs)
{
    std::vector<TypeDumpDifference> differences;
    DifferenceList list(differences);
    auto const classes = PairItems(SortedClasses(ours), SortedClasses(theirs), ClassIdentity);
    for (auto const& [key, entry] : classes.Entries)
    {
        std::string const className = ClassLabel(entry.first != nullptr ? *entry.first : *entry.second);
        if (entry.first == nullptr || entry.second == nullptr)
        {
            list.AddPresence(className, {}, "class", entry.first != nullptr);
            continue;
        }
        CompareClasses(list, className, *entry.first, *entry.second);
    }
    return differences;
}

std::string TypeDumpWriter::Describe(TypeDumpDifference const& difference)
{
    std::string subject = Printable(difference.Class);
    if (!difference.Property.empty())
        subject += fmt::format(" property {}", Printable(difference.Property));
    bool const onlyOurs = difference.Ours == Present && difference.Theirs == Missing;
    bool const onlyTheirs = difference.Ours == Missing && difference.Theirs == Present;
    if (onlyOurs || onlyTheirs)
    {
        std::string_view const side = onlyOurs ? "ours" : "theirs";
        if (difference.Field == "class" || difference.Field == "property")
            return fmt::format("{}: only in {}", subject, side);
        return fmt::format("{}: {} only in {}", subject, Printable(difference.Field), side);
    }
    return fmt::format("{}: {} ours {}, theirs {}", subject, Printable(difference.Field), Printable(difference.Ours), Printable(difference.Theirs));
}
