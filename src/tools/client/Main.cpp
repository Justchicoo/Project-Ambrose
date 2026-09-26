/*
 * Project Ambrose by Imjustchico
 * Asks the user's own Wizard101 install a question and prints the answer. One tool rather than one per question, because every one of them needs the same three things first, the install, its type dump and an archive out of it, and a question nobody can ask is a wall that stops a milestone rather than a gap in a list. `types` searches and prints the classes the dump holds, which is the only way to read it at all: it is keyed by hash, so no search of the file itself finds a name; given a hash the dump does not list, it reads the client program itself for a name that hashes to it, including the mangled form the runtime keeps class names in, where a leading AV or AU stands for class or struct, so an unknown class is reported by name rather than as a number nobody can act on. `messages` prints what the client says a message carries, read from the client's own XML rather than from anybody's notes, under the protocol, service and order the servers give it, worked out by the same definition code they load the XML with, so the numbers a capture or a log shows can be matched to a name without counting tags by hand. `handlers` says which classes in the client program handle a message and where, found the way the program registers them: each handler goes in under a debug name such as WizardGraphicalClient::MSG_TimedAccessPasses, with its plain name and a pointer to the function, so client-image finds the code that reads both names and the function address it loads, and the answer is written once per revision; a message nothing registers that way is one the client only sends or registers some other way, which the tool says rather than guessing which. `behaviors` says which class the client program builds for each behavior it registers, by following each behavior's name to the factory the program stores for it, the vtable that factory's create function gives the object and the class name its GetType registers, with the bases each GetType registers its class under, which is how a behavior a template names is known to become a given client class without a capture of it; when the dump does not list that class, the nearest base it does list is named, since that is the class whose properties the dump can describe. `template` prints the object template an id names, found through TemplateManifest.xml the way the client and the game server find it, in Root.wad or in the World-Part.wad a path written |World|Part|path names, with its file, archive, object name and behaviors, then the template itself, so a zone object's template id can be read without searching the manifest by hand; it reads them through the game server's own template store, and its list prints every id the manifest holds with the archive and entry it names, marking each the install lacks, which is how a streamed archive not yet fetched shows up. `lang` prints the text behind a locale key, because most of the client's data carries an id where a person expects words, and searches the keys by the text they hold. `wad` lists and prints archive entries, BINd as JSON, an object stored with no BINd header as JSON too, which is how a zone's gamedata.bin is kept, and anything else as the text it holds. `core` prints a game object blob, what MSG_LOGINCOMPLETE and MSG_NEWOBJECT carry, whose every object opens with the client's CoreObject header, a block, a type and a template id, rather than a class hash; it opens the envelope itself when there is one, reads the block and type pairs the world database's core_object_type holds when it is given the world database, and when a pair stands for a class nobody has named yet it lists the classes the dump derives from CoreObject rather than guessing, so the one that decodes can be named with --pair or, for the root, --as. Given the world database, every command also reads the classes its server_class tables describe for the dump, and types marks them as coming from there. Reading a headerless object needs no flag because it proves itself: the bytes decode only if they open with a class hash the dump knows and the whole object parses, so a wrong guess refuses rather than printing rubble. Each command is meant to grow and new ones to join them, so the next thing the client work needs is taught here rather than worked around where it was needed. What this install's messages carry is written once to the Ambrose data folder and read from there afterwards, and a type dump is read through the fast copy beside it, which is built once if it is not there, so asking a second question costs a fraction of the first rather than the same six seconds again.
 */

#include "BehaviorFactories.h"
#include "BindFile.h"
#include "CodeIndex.h"
#include "BlobEnvelope.h"
#include "CoreObjectSerializer.h"
#include "DatabaseEnv.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "ObjectSerializer.h"
#include "PeImage.h"
#include "ClientLocator.h"
#include "Environment.h"
#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "MessageDefinitionSet.h"
#include "LocaleStore.h"
#include "MessageHandlers.h"
#include "StringHash.h"
#include "Log.h"
#include "LogConfig.h"
#include "PropertyJson.h"
#include "StringUtil.h"
#include "TypeDumpLoader.h"
#include "TypeRegistry.h"
#include "TypeRegistryBinary.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;
    constexpr std::size_t ListedByDefault = 40;

    constexpr std::string_view Usage = R"(Usage: client <command> [options] [argument]...

Asks your own Wizard101 install a question and prints the answer.

Commands:
  types <pattern>...     print every class whose name holds a pattern, or a hash
  types --list <pattern> print only the names, one per line
  messages <tag>...      print what the client says a message carries, with its
                         protocol, service and order
  messages --list [text] print every message tag with its service and order, or those
                         holding the text
  handlers <tag>...      print which classes in the client program handle a message
                         and the address of each handler, or name a class or a
                         handler name instead of a tag
  handlers --list [text] print every handler the client program registers
  template <id>...       print the object template an id names, found through
                         TemplateManifest.xml, with its archive, file, name and behaviors
  template --list [text] print every template id the manifest lists with its archive and
                         entry, or those holding the text, marking each the install lacks
  behaviors <name>...    print the class the client program builds for a behavior it
                         registers, and where it found each link
  behaviors --list [text] print every behavior the client program registers
  wad <entry>...         print an archive entry: BINd and headerless objects as JSON,
                         the rest as text
  wad --list [pattern]   print entry names holding the pattern
  lang <key>             print the text a locale key holds, which is what the client
                         shows where the data carries only an id
  lang --list [text]     print the keys whose text holds the pattern
  core <file>...         print a game object blob, the Data a MSG_LOGINCOMPLETE or a
                         MSG_NEWOBJECT carries, enveloped or not, with the block, type
                         and template its header names; --as names the class they stand for
  hex <file>...          print a file's bytes with their offsets, from --from for --count

Options:
  --client <dir>       the install to read (default: AMBROSE_CLIENT_DIR)
  --type-dump <file>   the type dump made from it (default: AMBROSE_TYPE_DUMP_PATH)
  --wad <file>         the archive wad reads (default: Root.wad)
  --locale <name>      the locale lang reads (default: en-US)
  --all                print every match rather than the first few
  --as <class>         the class a game object's block and type stand for, such as
                       "class WizClientObject"
  --trailing           let core stop where the class ends and go on to read each object
                       that follows it the same way, as MSG_LOGINCOMPLETE's Data holds the
                       player and then its stats, saying where bytes are left that do not
                       read, which is how a class that ends early is told from a wrong one
  --mask <n>           read core with this property flag mask rather than the one the
                       server sends a player's own object with, Transmit|AuthorityTransmit
                       (decimal or 0x hex)
  --pair <b>:<t>=<class>
                       the class a block and type stand for wherever they appear in a
                       core blob, such as 115:9="class WizClientObjectItem"; repeatable,
                       and it takes the place of the world database's row for that pair
  --world-db <info>    the world database, host;port;user;password;database, whose
                       server_class and core_object_type rows join what the dump says
                       (default: AMBROSE_WORLD_DATABASE_INFO)
  --flags <n>          read core with exactly these serializer flags rather than trying
                       the plain form and then a stream that opens with its own flags
  --from <n>, --count <n>
                       where hex starts and how many bytes it prints (decimal or 0x hex)
  --help               print this text

Exit status: 0 when every question was answered, 1 when one was not, 2 on bad usage.
)";

    struct Arguments
    {
        std::string Command;
        std::optional<std::string> Client;
        std::optional<std::string> TypeDump;
        std::string Wad = "Root.wad";
        std::string Locale = "en-US";
        std::optional<std::string> As;
        std::vector<std::string> Pairs;
        std::optional<std::string> WorldDatabase;
        std::size_t From = 0;
        std::size_t Count = 0;
        bool List = false;
        bool All = false;
        bool Trailing = false;
        std::optional<uint32> Flags;
        std::optional<uint32> Mask;
        bool Help = false;
        std::vector<std::string> Subjects;
    };

    void BuildBinaryCache(std::filesystem::path const& json, std::filesystem::path const& binary)
    {
        std::ifstream stream(json, std::ios::binary);
        if (!stream)
            return;
        std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (stream.bad())
            return;
        TypeDumpLoader::RawDump dump;
        std::vector<std::string> errors;
        if (!TypeDumpLoader::Parse(text, dump, errors))
            return;
        std::string error;
        if (TypeRegistryBinary::Write(binary, dump, json.stem().string(), error))
            std::cerr << fmt::format("client: built the fast copy of this type dump at {}, so every later question reads it instead of the JSON\n",
                ConfigMgr::PathToUtf8(binary));
    }

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments parsed;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            auto const value = [&](std::string_view option) -> std::optional<std::string>
            {
                if (index + 1 >= args.size())
                {
                    error = fmt::format("{} needs a value", option);
                    return std::nullopt;
                }
                return args[++index];
            };
            if (arg == "--help" || arg == "-h")
                parsed.Help = true;
            else if (arg == "--list")
                parsed.List = true;
            else if (arg == "--trailing")
                parsed.Trailing = true;
            else if (arg == "--pair")
            {
                std::optional<std::string> const given = value(arg);
                if (!given)
                    return std::nullopt;
                parsed.Pairs.push_back(*given);
            }
            else if (arg == "--flags" || arg == "--mask")
            {
                std::optional<std::string> const given = value(arg);
                if (!given)
                    return std::nullopt;
                std::string_view const digits = *given;
                bool const hexadecimal = digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X');
                std::optional<uint32> const number = hexadecimal ? Ambrose::StringTo<uint32>(digits.substr(2), 16) : Ambrose::StringTo<uint32>(digits, 10);
                if (!number)
                {
                    error = arg == "--flags" ? "--flags takes the serializer flags as a number, decimal or 0x hex" : "--mask takes the property flag mask as a number, decimal or 0x hex";
                    return std::nullopt;
                }
                (arg == "--flags" ? parsed.Flags : parsed.Mask) = *number;
            }
            else if (arg == "--all")
                parsed.All = true;
            else if (arg == "--locale")
            {
                if (index + 1 >= args.size())
                {
                    error = "--locale needs a locale name";
                    return std::nullopt;
                }
                parsed.Locale = args[++index];
            }
            else if (arg == "--as")
            {
                std::optional<std::string> const given = value(arg);
                if (!given)
                    return std::nullopt;
                parsed.As = *given;
            }
            else if (arg == "--from" || arg == "--count")
            {
                std::optional<std::string> const given = value(arg);
                if (!given)
                    return std::nullopt;
                std::string_view const digits = *given;
                bool const hexadecimal = digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X');
                std::optional<uint64> const number = hexadecimal ? Ambrose::StringTo<uint64>(digits.substr(2), 16) : Ambrose::StringTo<uint64>(digits, 10);
                if (!number)
                {
                    error = fmt::format("{} takes a number of bytes, decimal or 0x hex", arg);
                    return std::nullopt;
                }
                (arg == "--from" ? parsed.From : parsed.Count) = static_cast<std::size_t>(*number);
            }
            else if (arg == "--client" || arg == "--type-dump" || arg == "--wad" || arg == "--world-db")
            {
                std::optional<std::string> const given = value(arg);
                if (!given)
                    return std::nullopt;
                if (arg == "--client")
                    parsed.Client = *given;
                else if (arg == "--type-dump")
                    parsed.TypeDump = *given;
                else if (arg == "--world-db")
                    parsed.WorldDatabase = *given;
                else
                    parsed.Wad = *given;
            }
            else if (arg.starts_with("--"))
            {
                error = fmt::format("there is no option {}", arg);
                return std::nullopt;
            }
            else if (parsed.Command.empty())
                parsed.Command = arg;
            else
                parsed.Subjects.push_back(arg);
        }
        return parsed;
    }

    std::string Describe(PropertyInfo const& property)
    {
        std::string text = fmt::format("    {} {}", property.TypeName.empty() ? std::string("?") : property.TypeName, property.Name);
        if (property.Container != ContainerKind::Static)
            text += " []";
        if (property.Pointer)
            text += " *";
        text += fmt::format("  id {} offset {} hash {}", property.Id, property.Offset, property.Hash);
        if (property.Flags != 0)
        {
            static constexpr std::pair<PropertyFlag, std::string_view> Named[] = {
                { PropertyFlag::Save, "Save" },
                { PropertyFlag::Copy, "Copy" },
                { PropertyFlag::Public, "Public" },
                { PropertyFlag::Transmit, "Transmit" },
                { PropertyFlag::AuthorityTransmit, "AuthorityTransmit" },
                { PropertyFlag::Persistent, "Persistent" },
                { PropertyFlag::Deprecated, "Deprecated" },
                { PropertyFlag::NoScript, "NoScript" },
                { PropertyFlag::DirtyEncode, "DirtyEncode" },
                { PropertyFlag::Blob, "Blob" },
                { PropertyFlag::Immutable, "Immutable" },
                { PropertyFlag::FileName, "FileName" },
                { PropertyFlag::Color, "Color" },
                { PropertyFlag::Bits, "Bits" },
                { PropertyFlag::Enum, "Enum" },
                { PropertyFlag::Localized, "Localized" },
                { PropertyFlag::StringKey, "StringKey" },
                { PropertyFlag::ObjectId, "ObjectId" },
                { PropertyFlag::ReferenceId, "ReferenceId" },
                { PropertyFlag::ObjectName, "ObjectName" },
                { PropertyFlag::HasBaseClass, "HasBaseClass" },
            };
            std::string names;
            uint32 known = 0;
            for (auto const& [flag, name] : Named)
                if (property.HasFlag(flag))
                {
                    names += names.empty() ? "" : "|";
                    names += name;
                    known |= PropertyFlags::Bit(flag);
                }
            if ((property.Flags & ~known) != 0)
                names += fmt::format("{}{:#x}", names.empty() ? "" : "|", property.Flags & ~known);
            text += fmt::format("  flags {}", names);
        }
        if (!property.Options.empty())
        {
            text += "  {";
            for (std::size_t index = 0; index < property.Options.size(); ++index)
            {
                if (index != 0)
                    text += ", ";
                text += fmt::format("{}={}", property.Options[index].Name, property.Options[index].Value);
            }
            text += "}";
        }
        return text;
    }

    void Print(ClassInfo const& info)
    {
        std::cout << fmt::format("{}  hash {}{}\n", info.Name, info.Hash, sTypeRegistry.IsFromSupplement(info.Hash) ? fmt::format("  from {}", ObjectSchemaMgr::ClassSource) : std::string());
        if (!info.Bases.empty())
        {
            std::cout << "  bases:";
            for (ClassInfo const* base : info.Bases)
                std::cout << " " << (base != nullptr ? base->Name : std::string("?"));
            std::cout << "\n";
        }
        std::cout << fmt::format("  {} propert{}\n", info.Properties.size(), info.Properties.size() == 1 ? "y" : "ies");
        for (PropertyInfo const& property : info.Properties)
            std::cout << Describe(property) << "\n";
    }

    std::optional<std::string> NameOfHash(std::filesystem::path const& clientDir, uint32 hash)
    {
        std::filesystem::path const program = clientDir / "Bin" / "WizardGraphicalClient.exe";
        std::ifstream stream(program, std::ios::binary);
        if (!stream)
            return std::nullopt;
        std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (stream.bad())
            return std::nullopt;

        auto const carries = [](unsigned char c)
        {
            return std::isalnum(c) != 0 || c == '_' || c == ':' || c == '<' || c == '>' || c == ',' || c == ' ' || c == '*';
        };
        std::size_t start = 0;
        while (start < bytes.size())
        {
            if (!carries(static_cast<unsigned char>(bytes[start])))
            {
                ++start;
                continue;
            }
            std::size_t end = start;
            while (end < bytes.size() && carries(static_cast<unsigned char>(bytes[end])))
                ++end;
            if (end - start >= 3 && end - start <= 120)
            {
                std::string_view const text(bytes.data() + start, end - start);
                std::string_view const stems[] = { text, text.substr(std::min<std::size_t>(2, text.size())) };
                for (std::string_view const stem : stems)
                {
                    if (stem.empty())
                        continue;
                    for (std::string_view const prefix : { std::string_view(), std::string_view("class "), std::string_view("struct "), std::string_view("enum ") })
                    {
                        std::string candidate;
                        candidate.reserve(prefix.size() + stem.size());
                        candidate.append(prefix).append(stem);
                        if (StringHash::KiStringHash(candidate) == hash)
                            return candidate;
                    }
                }
            }
            start = end;
        }
        return std::nullopt;
    }

    int RunTypes(Arguments const& arguments, TypeCatalog const& catalog, std::filesystem::path const& clientDir)
    {
        if (arguments.Subjects.empty())
        {
            std::cerr << "client types needs a name, part of one, or a hash\n";
            return BadUsage;
        }

        int status = Success;
        for (std::string const& subject : arguments.Subjects)
        {
            if (std::optional<uint32> const hash = Ambrose::StringTo<uint32>(subject))
            {
                if (ClassInfo const* const found = catalog.FindClass(*hash))
                {
                    Print(*found);
                    continue;
                }
                if (!clientDir.empty())
                {
                    if (std::optional<std::string> const name = NameOfHash(clientDir, *hash))
                    {
                        std::cout << fmt::format("{}\n", *name);
                        std::cerr << fmt::format("client: the type dump does not list {}, but the client program names it, so the dump is missing a class rather than the hash being wrong\n", *name);
                        status = Failure;
                        continue;
                    }
                }
            }
            if (ClassInfo const* const exact = catalog.FindClass(subject))
            {
                if (arguments.List)
                    std::cout << exact->Name << "\n";
                else
                    Print(*exact);
                continue;
            }

            std::string const wanted = Ambrose::ToLower(subject);
            std::vector<ClassInfo const*> matches;
            for (ClassInfo const* const info : catalog.GetClasses())
                if (info != nullptr && Ambrose::ToLower(info->Name).find(wanted) != std::string::npos)
                    matches.push_back(info);

            if (matches.empty())
            {
                std::cerr << fmt::format("{}: the type dump holds no class with that name or hash\n", subject);
                status = Failure;
                continue;
            }

            std::size_t const shown = arguments.All || arguments.List ? matches.size() : std::min<std::size_t>(matches.size(), ListedByDefault);
            for (std::size_t index = 0; index < shown; ++index)
            {
                if (arguments.List || matches.size() > 1)
                    std::cout << matches[index]->Name << "\n";
                else
                    Print(*matches[index]);
            }
            if (shown < matches.size())
                std::cout << fmt::format("... {} more; pass --all to print them\n", matches.size() - shown);
        }
        return status;
    }

    std::optional<std::string> AsText(std::span<uint8 const> data)
    {
        std::string text;
        text.reserve(data.size());
        for (uint8 const byte : data)
        {
            if (byte == 0 || (byte < 0x20 && byte != '\t' && byte != '\n' && byte != '\r'))
                return std::nullopt;
            text.push_back(static_cast<char>(byte));
        }
        return text;
    }

    struct CachedMessage
    {
        std::string Tag;
        std::string File;
        std::string Text;
        std::string Protocol;
        uint32 Service = 0;
        uint32 Order = 0;
        std::string Handler;
    };

    std::filesystem::path MessageCachePath(std::filesystem::path const& dataFolder, std::string_view revision)
    {
        return dataFolder / "messages" / (std::string(revision) + ".json");
    }

    std::vector<CachedMessage> ReadMessageCache(std::filesystem::path const& path)
    {
        std::vector<CachedMessage> messages;
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return messages;
        nlohmann::json document;
        try
        {
            stream >> document;
        }
        catch (std::exception const&)
        {
            return messages;
        }
        if (!document.is_array())
            return messages;
        for (nlohmann::json const& entry : document)
        {
            if (!entry.is_object() || !entry.contains("tag") || !entry.contains("file") || !entry.contains("text") || !entry.contains("protocol") || !entry.contains("service")
                || !entry.contains("order") || !entry.contains("handler"))
                return {};
            messages.push_back({ entry["tag"].get<std::string>(), entry["file"].get<std::string>(), entry["text"].get<std::string>(), entry["protocol"].get<std::string>(),
                entry["service"].get<uint32>(), entry["order"].get<uint32>(), entry["handler"].get<std::string>() });
        }
        return messages;
    }

    void WriteMessageCache(std::filesystem::path const& path, std::vector<CachedMessage> const& messages)
    {
        std::error_code code;
        std::filesystem::create_directories(path.parent_path(), code);
        if (code)
            return;
        nlohmann::json document = nlohmann::json::array();
        for (CachedMessage const& message : messages)
            document.push_back({ { "tag", message.Tag }, { "file", message.File }, { "text", message.Text }, { "protocol", message.Protocol }, { "service", message.Service },
                { "order", message.Order }, { "handler", message.Handler } });
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return;
        stream << document.dump(1, '\t');
        if (stream.good())
            std::cerr << fmt::format("client: wrote what this install's {} messages carry to {}, so every later question reads it instead of the archive\n",
                messages.size(), ConfigMgr::PathToUtf8(path));
    }

    std::vector<std::string> MessageFiles(KiwadArchive const& archive)
    {
        std::vector<std::string> files;
        for (KiwadEntry const& entry : archive.GetEntries())
            if (entry.Name.find("Messages") != std::string::npos && entry.Name.ends_with(".xml"))
                files.push_back(entry.Name);
        std::sort(files.begin(), files.end());
        return files;
    }

    std::vector<CachedMessage> GatherMessages(KiwadArchive const& archive)
    {
        std::vector<CachedMessage> messages;
        for (std::string const& file : MessageFiles(archive))
        {
            KiwadReadResult const read = archive.Read(file);
            if (!read.Succeeded())
                continue;
            std::optional<std::string> const text = AsText(read.Data);
            if (!text)
                continue;
            std::size_t position = 0;
            while ((position = text->find("<MSG_", position)) != std::string::npos)
            {
                std::size_t const nameEnd = text->find('>', position);
                if (nameEnd == std::string::npos)
                    break;
                std::string const tag = text->substr(position + 1, nameEnd - position - 1);
                std::size_t const close = text->find("</" + tag + ">", nameEnd);
                if (close == std::string::npos)
                {
                    position = nameEnd + 1;
                    continue;
                }
                messages.push_back({ tag, file, text->substr(position, close + tag.size() + 3 - position), {}, 0, 0, {} });
                position = close + 1;
            }
        }

        MessageDefinitionSet definitions;
        definitions.LoadFromArchive(archive);
        std::map<std::pair<std::string, std::string>, std::tuple<std::string, uint32, uint32, std::string>> numbered;
        for (auto const& [service, protocol] : definitions.GetProtocols())
            for (MessageDef const& definition : protocol.Messages)
                numbered[{ std::filesystem::path(protocol.SourceFile).filename().string(), definition.Tag }] = { protocol.ProtocolType, service, definition.Order, definition.Handler };
        for (CachedMessage& message : messages)
        {
            auto const found = numbered.find({ std::filesystem::path(message.File).filename().string(), message.Tag });
            if (found == numbered.end())
                continue;
            std::tie(message.Protocol, message.Service, message.Order, message.Handler) = found->second;
        }
        return messages;
    }

    std::string Numbered(CachedMessage const& message)
    {
        if (message.Protocol.empty())
            return fmt::format("{} (no order: the definition code refused its file)", message.Tag);
        return fmt::format("{} {} ({}:{})", message.Protocol, message.Tag, message.Service, message.Order);
    }

    int RunMessages(Arguments const& arguments, std::vector<CachedMessage> const& messages)
    {
        if (messages.empty())
        {
            std::cerr << "this install holds no message definitions\n";
            return Failure;
        }

        std::vector<std::string> wanted;
        for (std::string const& subject : arguments.Subjects)
            wanted.push_back(Ambrose::ToLower(subject));
        if (wanted.empty())
            wanted.emplace_back();

        int status = Success;
        for (std::string const& subject : wanted)
        {
            bool found = false;
            for (CachedMessage const& message : messages)
            {
                std::string const tag = Ambrose::ToLower(message.Tag);
                bool const matches = arguments.List ? (subject.empty() || tag.find(subject) != std::string::npos)
                                                    : (tag == subject || (!subject.empty() && tag.find(subject) != std::string::npos));
                if (!matches)
                    continue;
                found = true;
                if (arguments.List)
                    std::cout << fmt::format("{}  {}\n", Numbered(message), message.File);
                else
                    std::cout << fmt::format("{} in {}\n{}\n", Numbered(message), message.File, message.Text);
            }
            if (!found && !arguments.List)
            {
                std::cerr << fmt::format("{}: no message of that name is defined in this install\n", subject);
                status = Failure;
            }
        }
        return status;
    }

    std::filesystem::path HandlerCachePath(std::filesystem::path const& dataFolder, std::string_view revision)
    {
        return dataFolder / "handlers" / (std::string(revision) + ".json");
    }

    constexpr uint32 HandlerFinderVersion = 5;

    std::vector<MessageHandlerRegistration> ReadHandlerCache(std::filesystem::path const& path)
    {
        std::vector<MessageHandlerRegistration> registrations;
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return registrations;
        nlohmann::json document;
        try
        {
            stream >> document;
        }
        catch (std::exception const&)
        {
            return registrations;
        }
        if (!document.is_object() || !document.contains("finder") || document["finder"] != HandlerFinderVersion || !document.contains("registrations")
            || !document["registrations"].is_array())
            return registrations;
        for (nlohmann::json const& entry : document["registrations"])
        {
            if (!entry.is_object() || !entry.contains("owner") || !entry.contains("handler") || !entry.contains("site") || !entry.contains("address"))
                return {};
            registrations.push_back({ entry["owner"].get<std::string>(), entry["handler"].get<std::string>(), entry["site"].get<uint64>(), entry["address"].get<uint64>() });
        }
        return registrations;
    }

    void WriteHandlerCache(std::filesystem::path const& path, std::vector<MessageHandlerRegistration> const& registrations)
    {
        std::error_code code;
        std::filesystem::create_directories(path.parent_path(), code);
        if (code)
            return;
        nlohmann::json list = nlohmann::json::array();
        for (MessageHandlerRegistration const& registration : registrations)
            list.push_back({ { "owner", registration.Owner }, { "handler", registration.Handler }, { "site", registration.Site }, { "address", registration.Address } });
        nlohmann::json const document = { { "finder", HandlerFinderVersion }, { "registrations", std::move(list) } };
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return;
        stream << document.dump(1, '\t');
        if (stream.good())
            std::cerr << fmt::format("client: wrote the {} message handlers this install's client program registers to {}, so every later question reads it instead of the program\n",
                registrations.size(), ConfigMgr::PathToUtf8(path));
    }

    std::string DescribeRegistration(MessageHandlerRegistration const& registration)
    {
        if (registration.Address == 0)
            return fmt::format("  {}::{}  registered at 0x{:x}; no function address is loaded before it", registration.Owner, registration.Handler, registration.Site);
        return fmt::format("  {}::{}  handler 0x{:x}, registered at 0x{:x}", registration.Owner, registration.Handler, registration.Address, registration.Site);
    }

    int RunHandlers(Arguments const& arguments, std::vector<CachedMessage> const& messages, std::vector<MessageHandlerRegistration> const& registrations)
    {
        if (arguments.List)
        {
            std::string const pattern = arguments.Subjects.empty() ? std::string() : Ambrose::ToLower(arguments.Subjects.front());
            std::size_t shown = 0;
            for (MessageHandlerRegistration const& registration : registrations)
            {
                if (!pattern.empty() && Ambrose::ToLower(registration.Owner + "::" + registration.Handler).find(pattern) == std::string::npos)
                    continue;
                std::cout << DescribeRegistration(registration).substr(2) << "\n";
                ++shown;
            }
            std::cerr << fmt::format("client: {} of the {} handler registrations in the client program match\n", shown, registrations.size());
            return shown == 0 ? Failure : Success;
        }

        int status = Success;
        for (std::string const& subject : arguments.Subjects)
        {
            std::string const wanted = Ambrose::ToLower(subject);
            bool answered = false;
            std::set<std::string> shownHandlers;
            for (CachedMessage const& message : messages)
            {
                if (Ambrose::ToLower(message.Tag) != wanted || message.Handler.empty() || !shownHandlers.insert(message.Handler).second)
                    continue;
                answered = true;
                std::cout << fmt::format("{} is handled as {}\n", Numbered(message), message.Handler);
                bool registered = false;
                for (MessageHandlerRegistration const& registration : registrations)
                {
                    if (registration.Handler != message.Handler)
                        continue;
                    registered = true;
                    std::cout << DescribeRegistration(registration) << "\n";
                }
                if (!registered)
                    std::cout << "  no Class::MSG_Name registration in the client program names it; the client registers some handlers another way, which this tool does not find yet\n";
            }
            if (answered)
                continue;
            for (MessageHandlerRegistration const& registration : registrations)
            {
                if (Ambrose::ToLower(registration.Handler) != wanted && Ambrose::ToLower(registration.Owner) != wanted
                    && Ambrose::ToLower(registration.Owner + "::" + registration.Handler) != wanted)
                    continue;
                answered = true;
                std::cout << DescribeRegistration(registration).substr(2) << "\n";
            }
            if (!answered)
            {
                std::cerr << fmt::format("{}: no message, class or handler of that name is known to this install\n", subject);
                status = Failure;
            }
        }
        return status;
    }

    constexpr uint32 BehaviorFinderVersion = 1;

    std::filesystem::path BehaviorCachePath(std::filesystem::path const& dataFolder, std::string_view revision)
    {
        return dataFolder / "behaviors" / (std::string(revision) + ".json");
    }

    std::vector<BehaviorFactory> ReadBehaviorCache(std::filesystem::path const& path)
    {
        std::vector<BehaviorFactory> factories;
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return factories;
        nlohmann::json document;
        try
        {
            stream >> document;
        }
        catch (std::exception const&)
        {
            return factories;
        }
        if (!document.is_object() || !document.contains("finder") || document["finder"] != BehaviorFinderVersion || !document.contains("behaviors")
            || !document["behaviors"].is_array())
            return factories;
        for (nlohmann::json const& entry : document["behaviors"])
        {
            if (!entry.is_object() || !entry.contains("behavior") || !entry.contains("class") || !entry.contains("site") || !entry.contains("factory") || !entry.contains("create")
                || !entry.contains("object") || !entry.contains("get_type") || !entry.contains("bases") || !entry["bases"].is_array())
                return {};
            factories.push_back({ entry["behavior"].get<std::string>(), entry["site"].get<uint64>(), entry["factory"].get<uint64>(), entry["create"].get<uint64>(),
                entry["object"].get<uint64>(), entry["get_type"].get<uint64>(), entry["class"].get<std::string>(), entry["bases"].get<std::vector<std::string>>() });
        }
        return factories;
    }

    void WriteBehaviorCache(std::filesystem::path const& path, std::vector<BehaviorFactory> const& factories)
    {
        std::error_code code;
        std::filesystem::create_directories(path.parent_path(), code);
        if (code)
            return;
        nlohmann::json list = nlohmann::json::array();
        for (BehaviorFactory const& factory : factories)
            list.push_back({ { "behavior", factory.Behavior }, { "class", factory.ClassName }, { "site", factory.Site }, { "factory", factory.FactoryVtable },
                { "create", factory.Create }, { "object", factory.ObjectVtable }, { "get_type", factory.GetType }, { "bases", factory.Bases } });
        nlohmann::json const document = { { "finder", BehaviorFinderVersion }, { "behaviors", std::move(list) } };
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return;
        stream << document.dump(1, '\t');
        if (stream.good())
            std::cerr << fmt::format("client: wrote the {} behaviors this install's client program registers to {}, so every later question reads it instead of the program\n",
                factories.size(), ConfigMgr::PathToUtf8(path));
    }

    std::optional<std::string> DumpNameOf(TypeCatalog const* catalog, std::string const& className)
    {
        if (catalog == nullptr || className.empty())
            return std::nullopt;
        for (std::string_view const prefix : { std::string_view("class "), std::string_view("struct ") })
            if (catalog->FindClass(std::string(prefix) + className) != nullptr)
                return std::string(prefix) + className;
        return std::nullopt;
    }

    std::string ClassOf(BehaviorFactory const& factory, TypeCatalog const* catalog)
    {
        if (factory.ClassName.empty())
            return "class not found";
        if (catalog == nullptr)
            return factory.ClassName;
        if (std::optional<std::string> const listed = DumpNameOf(catalog, factory.ClassName))
            return *listed;
        for (std::string const& base : factory.Bases)
            if (std::optional<std::string> const listed = DumpNameOf(catalog, base))
                return fmt::format("{}, which the type dump does not list, a {}", factory.ClassName, *listed);
        return fmt::format("{}, which the type dump does not list, nor any of its bases", factory.ClassName);
    }

    std::string DescribeFactory(BehaviorFactory const& factory, TypeCatalog const* catalog)
    {
        return fmt::format("{}  {}  (factory 0x{:x}, create 0x{:x}, object vtable 0x{:x}, GetType 0x{:x}, read at 0x{:x})", factory.Behavior, ClassOf(factory, catalog),
            factory.FactoryVtable, factory.Create, factory.ObjectVtable, factory.GetType, factory.Site);
    }

    int RunBehaviors(Arguments const& arguments, std::vector<BehaviorFactory> const& factories, TypeCatalog const* catalog)
    {
        if (arguments.List)
        {
            std::string const pattern = arguments.Subjects.empty() ? std::string() : Ambrose::ToLower(arguments.Subjects.front());
            std::size_t shown = 0;
            for (BehaviorFactory const& factory : factories)
            {
                if (!pattern.empty() && Ambrose::ToLower(fmt::format("{} {} {}", factory.Behavior, factory.ClassName, fmt::join(factory.Bases, " "))).find(pattern) == std::string::npos)
                    continue;
                std::cout << DescribeFactory(factory, catalog) << "\n";
                ++shown;
            }
            std::cerr << fmt::format("client: {} of the {} behaviors the client program registers match\n", shown, factories.size());
            return shown == 0 ? Failure : Success;
        }
        int status = Success;
        for (std::string const& subject : arguments.Subjects)
        {
            bool answered = false;
            for (BehaviorFactory const& factory : factories)
            {
                if (Ambrose::ToLower(factory.Behavior) != Ambrose::ToLower(subject))
                    continue;
                answered = true;
                std::cout << DescribeFactory(factory, catalog) << "\n";
                if (!factory.Bases.empty())
                    std::cout << fmt::format("  {} derives from {}\n", factory.ClassName, fmt::join(factory.Bases, ", "));
            }
            if (!answered)
            {
                std::cerr << fmt::format("{}: the client program registers no factory this tool can follow for a behavior of that name\n", subject);
                status = Failure;
            }
        }
        return status;
    }

    int RunHex(Arguments const& arguments)
    {
        int status = Success;
        for (std::string const& subject : arguments.Subjects)
        {
            std::ifstream stream(LogConfig::Utf8Path(subject), std::ios::binary);
            if (!stream)
            {
                std::cerr << fmt::format("{}: cannot be read\n", subject);
                status = Failure;
                continue;
            }
            std::vector<uint8> const bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            std::size_t const from = std::min(arguments.From, bytes.size());
            std::size_t const to = arguments.Count == 0 ? bytes.size() : std::min(bytes.size(), from + arguments.Count);
            for (std::size_t line = from; line < to; line += 16)
            {
                std::string hex;
                std::string text;
                for (std::size_t at = line; at < line + 16; ++at)
                {
                    if (at < to)
                    {
                        hex += fmt::format("{:02x} ", bytes[at]);
                        text += bytes[at] >= 0x20 && bytes[at] < 0x7F ? static_cast<char>(bytes[at]) : '.';
                    }
                    else
                        hex += "   ";
                    if (at == line + 7)
                        hex += ' ';
                }
                std::cout << fmt::format("{:08x}  {} {}\n", line, hex, text);
            }
        }
        return status;
    }

    int RunCore(Arguments const& arguments, TypeCatalogPtr const& catalog)
    {
        ClassInfo const* named = nullptr;
        if (arguments.As)
        {
            named = catalog->FindClass(*arguments.As);
            if (!named || named->Kind != ClassKind::PropertyClass)
            {
                std::cerr << fmt::format("{} is not a property class the type dump lists\n", *arguments.As);
                return Failure;
            }
        }
        std::vector<CoreObjectType> rows;
        CoreObjectTypeTablePtr const known = sObjectSchemaMgr.GetCoreObjectTypes();
        rows.assign(known->GetTypes().begin(), known->GetTypes().end());
        for (std::string const& text : arguments.Pairs)
        {
            std::size_t const colon = text.find(':');
            std::size_t const equals = text.find('=');
            std::optional<uint8> const block = colon == std::string::npos ? std::nullopt : Ambrose::StringTo<uint8>(std::string_view(text).substr(0, colon));
            std::optional<uint8> const type = colon == std::string::npos || equals == std::string::npos || equals < colon
                ? std::nullopt
                : Ambrose::StringTo<uint8>(std::string_view(text).substr(colon + 1, equals - colon - 1));
            if (!block || !type || equals + 1 >= text.size())
            {
                std::cerr << fmt::format("--pair {} is not <block>:<type>=<class>\n", text);
                return BadUsage;
            }
            std::erase_if(rows, [&](CoreObjectType const& row) { return row.Block == *block && row.Type == *type; });
            rows.push_back({ *block, *type, text.substr(equals + 1) });
        }
        int status = Success;
        for (std::string const& subject : arguments.Subjects)
        {
            std::ifstream stream(LogConfig::Utf8Path(subject), std::ios::binary);
            if (!stream)
            {
                std::cerr << fmt::format("{}: cannot be read\n", subject);
                status = Failure;
                continue;
            }
            std::vector<uint8> const bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            std::vector<uint8> inflated;
            if (bytes.size() > 4)
            {
                BlobEnvelope::UnwrapResult const opened = BlobEnvelope::Unwrap(bytes, SerializerLimits::Current().MaxInflatedSize);
                if (opened.Succeeded())
                    inflated.assign(opened.Data.begin(), opened.Data.end());
            }
            DecodeResult decoded;
            bool enveloped = false;
            bool decodedOnce = false;
            CoreObjectTypeTablePtr decodedTypes;
            SerializerOptions decodedOptions;
            std::vector<uint8> const* const payloads[] = { &bytes, &inflated };
            for (std::vector<uint8> const* const payload : payloads)
            {
                if (payload->size() < 2)
                    continue;
                std::vector<CoreObjectType> table = rows;
                uint8 const rootBlock = (*payload)[0];
                uint8 const rootType = (*payload)[1];
                bool const rootListed = std::any_of(table.begin(), table.end(), [&](CoreObjectType const& row) { return row.Block == rootBlock && row.Type == rootType; });
                if (named && !rootListed && (rootBlock != 0 || rootType != 0))
                    table.push_back({ rootBlock, rootType, named->Name });
                std::vector<std::string> tableErrors;
                CoreObjectTypeTablePtr const types = CoreObjectTypeTable::Build(std::move(table), *catalog, tableErrors);
                if (!types)
                {
                    for (std::string const& problem : tableErrors)
                        std::cerr << fmt::format("{}: {}\n", subject, problem);
                    return BadUsage;
                }
                std::vector<SerializerFlag> const tries = arguments.Flags ? std::vector<SerializerFlag>{ static_cast<SerializerFlag>(*arguments.Flags) }
                                                                         : std::vector<SerializerFlag>{ SerializerFlag::None, SerializerFlag::SerializeFlags };
                for (SerializerFlag const flags : tries)
                {
                    SerializerOptions options;
                    options.Flags = flags;
                    options.Mask = arguments.Mask.value_or(SerializerOptions::TransmitMask);
                    options.AllowTrailingBytes = arguments.Trailing;
                    DecodeResult attempt = CoreObjectSerializer::Decode(catalog, *payload, *types, options);
                    if (attempt.Ok() || !decodedOnce)
                    {
                        decoded = std::move(attempt);
                        enveloped = payload == &inflated;
                        decodedOnce = true;
                        decodedTypes = types;
                        decodedOptions = options;
                    }
                    if (decoded.Ok())
                        break;
                }
                if (decoded.Ok())
                    break;
            }
            if (!decoded.Ok())
            {
                if (decoded.UnknownCore)
                {
                    CoreObjectHeader const& missing = *decoded.UnknownCore;
                    std::cerr << fmt::format("{}: an object in it names block {} type {}; pass --pair {}:{}=<class>{} with the class they stand for, one of those the dump derives from CoreObject:\n",
                        subject, missing.Block, missing.Type, missing.Block, missing.Type, decoded.Header && decoded.Header->Block == missing.Block && decoded.Header->Type == missing.Type ? ", or --as <class>," : "");
                    if (ClassInfo const* const core = catalog->FindClass(CoreObjectTypeTable::CoreObjectClass))
                        for (ClassInfo const* const candidate : catalog->GetClasses())
                            if (candidate && candidate->Kind == ClassKind::PropertyClass && candidate != core && candidate->IsA(*core))
                                std::cerr << fmt::format("  {}\n", candidate->Name);
                }
                else
                    std::cerr << fmt::format("{}: {} {}\n", subject, ObjectSerializer::GetStatusName(decoded.Status), decoded.Detail);
                status = Failure;
                continue;
            }
            if (decoded.Header)
                std::cerr << fmt::format("{}: {}block {} type {} template {}{}, {} bytes read\n", subject, enveloped ? "enveloped, " : "",
                    decoded.Header->Block, decoded.Header->Type, decoded.Header->TemplateId,
                    decoded.StreamFlags ? fmt::format(", serializer flags {:#x}", *decoded.StreamFlags) : std::string(), decoded.BytesRead);
            std::cout << PropertyJson::Dump(decoded.Object.get(), 2) << "\n";
            if (!arguments.Trailing)
                continue;
            std::vector<uint8> const& data = enveloped ? inflated : bytes;
            std::size_t offset = decoded.BytesRead;
            while (offset < data.size())
            {
                DecodeResult const next = CoreObjectSerializer::Decode(catalog, std::span<uint8 const>(data).subspan(offset), *decodedTypes, decodedOptions);
                if (!next.Ok() || next.BytesRead == 0)
                {
                    std::cerr << fmt::format("{}: {} byte(s) from offset {:#x} do not read as another object, {} {}; client hex --from {:#x} shows them\n", subject,
                        data.size() - offset, offset, ObjectSerializer::GetStatusName(next.Status), next.Detail, offset);
                    status = Failure;
                    break;
                }
                std::cerr << fmt::format("{}: another object at offset {:#x}, {} bytes, block {} type {}{}\n", subject, offset, next.BytesRead, next.Header ? next.Header->Block : 0,
                    next.Header ? next.Header->Type : 0, next.Object ? fmt::format(", a {}", next.Object->GetClass().Name) : std::string(", null"));
                std::cout << PropertyJson::Dump(next.Object.get(), 2) << "\n";
                offset += next.BytesRead;
            }
        }
        return status;
    }

    int RunWad(Arguments const& arguments, KiwadArchive const& archive, TypeCatalogPtr const& catalog);

    int ListTemplates(Arguments const& arguments, TemplateManifest const& manifest, std::filesystem::path const& gameData)
    {
        std::map<std::string, std::unique_ptr<KiwadArchive>, std::less<>> archives;
        for (std::string const& name : manifest.GetArchives())
        {
            std::string error;
            std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(gameData / ConfigMgr::PathFromUtf8(name), error);
            if (!archive)
                std::cerr << fmt::format("client: {} cannot be opened: {}\n", name, error);
            archives.emplace(name, std::move(archive));
        }
        std::vector<std::pair<uint32, TemplateLocation const*>> sorted;
        sorted.reserve(manifest.Size());
        for (auto const& [id, location] : manifest.GetLocations())
            sorted.emplace_back(id, &location);
        std::sort(sorted.begin(), sorted.end(), [](auto const& left, auto const& right) { return left.first < right.first; });

        std::string const pattern = arguments.Subjects.empty() ? std::string() : Ambrose::ToLower(arguments.Subjects.front());
        std::size_t shown = 0;
        std::size_t missing = 0;
        for (auto const& [id, location] : sorted)
        {
            KiwadArchive const* const archive = archives.find(location->Archive)->second.get();
            bool const present = archive && archive->Find(location->Path);
            if (!present)
                ++missing;
            std::string const line = fmt::format("{}  {}  {}", id, location->Archive, location->Path);
            if (!pattern.empty() && Ambrose::ToLower(line).find(pattern) == std::string::npos)
                continue;
            std::cout << line << (present ? "" : archive ? "  (not in the archive)" : "  (the archive cannot be opened)") << "\n";
            ++shown;
        }
        std::cerr << fmt::format("client: {} of the {} templates {} lists in {} archives match; {} name an entry the install does not hold\n", shown, manifest.Size(),
            TemplateManifest::Entry, archives.size(), missing);
        return shown == 0 ? Failure : Success;
    }

    int RunTemplate(Arguments const& arguments, std::filesystem::path const& install, KiwadArchive const& archive, TypeCatalogPtr const& catalog)
    {
        ObjectTemplateMgr store;
        store.SetInstall(install);
        std::vector<std::string> errors;
        if (!store.LoadManifest(errors))
        {
            for (std::string const& problem : errors)
                std::cerr << fmt::format("client: {}\n", problem);
            return Failure;
        }
        std::filesystem::path const gameData = install / "Data" / "GameData";
        if (arguments.List)
            return ListTemplates(arguments, *store.GetManifest(), gameData);

        int status = Success;
        for (std::string const& subject : arguments.Subjects)
        {
            std::optional<uint32> const id = Ambrose::StringTo<uint32>(subject, 10);
            if (!id)
            {
                std::cerr << fmt::format("{}: a template is named by its id, a number\n", subject);
                status = Failure;
                continue;
            }
            TemplateLookup const found = store.Lookup(*id);
            if (!found.Template)
            {
                std::cerr << fmt::format("template {}: {}\n", *id, found.Error);
                status = Failure;
                continue;
            }
            ObjectTemplate const& object = *found.Template;
            std::cout << fmt::format("template {} is {} in {}, a {} named {}, with {} behavior(s): {}\n", object.TemplateId, object.File, object.Archive, object.Object->GetClass().Name,
                object.ObjectName.empty() ? std::string("nothing") : object.ObjectName, object.Behaviors.size(), fmt::join(object.Behaviors, ", "));
            bool const opened = ConfigMgr::PathToUtf8(archive.GetPath().filename()) == object.Archive;
            std::string error;
            std::unique_ptr<KiwadArchive> const other = opened ? nullptr : KiwadArchive::Open(gameData / ConfigMgr::PathFromUtf8(object.Archive), error);
            if (!opened && !other)
            {
                std::cerr << fmt::format("template {}: {} cannot be opened: {}\n", *id, object.Archive, error);
                status = Failure;
                continue;
            }
            Arguments entry = arguments;
            entry.List = false;
            entry.Subjects = { object.File };
            if (RunWad(entry, other ? *other : archive, catalog) != Success)
                status = Failure;
        }
        return status;
    }

    int RunWad(Arguments const& arguments, KiwadArchive const& archive, TypeCatalogPtr const& catalog)
    {
        if (arguments.List)
        {
            std::string const wanted = arguments.Subjects.empty() ? std::string() : arguments.Subjects.front();
            for (KiwadEntry const& entry : archive.GetEntries())
                if (wanted.empty() || entry.Name.find(wanted) != std::string::npos)
                    std::cout << entry.Name << "\n";
            return Success;
        }
        if (arguments.Subjects.empty())
        {
            std::cerr << "client wad needs an entry name, or --list\n";
            return BadUsage;
        }

        int status = Success;
        for (std::string const& name : arguments.Subjects)
        {
            KiwadReadResult const read = archive.Read(name);
            if (!read.Succeeded())
            {
                std::cerr << fmt::format("{}: {}\n", name, read.Error);
                status = Failure;
                continue;
            }
            if (catalog)
            {
                BindReadResult const result = BindFile::Read(catalog, read.Data);
                if (result.Ok())
                {
                    std::cout << PropertyJson::Dump(result.Decoded.Object.get(), 2) << "\n";
                    continue;
                }
                SerializerOptions options;
                options.Versionable = true;
                options.Flags = SerializerFlag::None;
                options.Mask = 0;
                options.Limits = BindFile::GetDefaultLimits();
                options.AllowNullRoot = false;
                options.AllowTrailingBytes = false;
                DecodeResult const raw = ObjectSerializer::Decode(catalog, read.Data, options);
                if (raw.Ok())
                {
                    std::cout << PropertyJson::Dump(raw.Object.get(), 2) << "\n";
                    continue;
                }
            }
            if (std::optional<std::string> const text = AsText(read.Data))
                std::cout << *text << (text->empty() || text->back() == '\n' ? "" : "\n");
            else
            {
                std::cerr << fmt::format("{}: this entry is neither a BINd object, a versionable object with no BINd header, nor text the dump describes\n", name);
                status = Failure;
            }
        }
        return status;
    }
}

int main(int argc, char** argv)
{
    sLog.SetLoggerLevel("root", LogLevel::Disabled);
    std::vector<std::string> const args = Ambrose::GetArguments(argc, argv);
    std::string error;
    std::optional<Arguments> arguments = Parse(args, error);
    if (!arguments)
    {
        std::cerr << error << "\n" << Usage;
        return BadUsage;
    }
    if (arguments->Help || arguments->Command.empty())
    {
        std::cout << Usage;
        return arguments->Help ? Success : BadUsage;
    }

    std::string const command = Ambrose::ToLower(arguments->Command);
    if (command == "hex")
    {
        if (arguments->Subjects.empty())
        {
            std::cerr << "client hex needs a file\n";
            return BadUsage;
        }
        return RunHex(*arguments);
    }

    if (command != "types" && command != "messages" && command != "handlers" && command != "behaviors" && command != "template" && command != "wad" && command != "lang"
        && command != "core")
    {
        std::cerr << fmt::format("there is no command {}\n{}", arguments->Command, Usage);
        return BadUsage;
    }

    if (arguments->Subjects.empty() && !arguments->List)
    {
        std::cerr << fmt::format("client {} needs something to look for; pass one, or --list to see what there is\n{}", command, Usage);
        return BadUsage;
    }

    bool const needsDump = command == "types" || command == "wad" || command == "core" || command == "template" || command == "behaviors";
    LocalClientSystem const system;
    SetupMode const mode = ClientSetup::ModeForTool(system, std::cerr, "client");
    std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ToolPrompt(std::cout, mode);
    std::optional<std::string> client = arguments->Client;
    ClientSetup::ForTool(mode, client, needsDump && !arguments->TypeDump ? &arguments->TypeDump : nullptr, *prompt, system,
        ClientSetup::ToolTypeDumps(system, "client", std::cerr), "client", std::cerr);
    arguments->Client = client;

    if (!arguments->WorldDatabase)
        arguments->WorldDatabase = Ambrose::GetEnv("AMBROSE_WORLD_DATABASE_INFO");
    bool const world = needsDump && arguments->WorldDatabase && !arguments->WorldDatabase->empty();
    if (world)
    {
        std::vector<std::string> errors;
        if (!WorldDatabase.SetConnectionInfo(*arguments->WorldDatabase, 1, 1) || WorldDatabase.Open() != 0)
        {
            std::cerr << "client: the world database --world-db names cannot be opened\n";
            return Failure;
        }
        if (!sObjectSchemaMgr.LoadClasses(errors))
        {
            for (std::string const& problem : errors)
                std::cerr << fmt::format("client: {}\n", problem);
            WorldDatabase.Close();
            return Failure;
        }
    }

    TypeCatalogPtr catalog;
    if (arguments->TypeDump && !arguments->TypeDump->empty())
    {
        std::filesystem::path const json = LogConfig::Utf8Path(*arguments->TypeDump);
        std::filesystem::path binary = json;
        binary.replace_extension(".bin");
        if (!std::filesystem::exists(binary) && std::filesystem::exists(json))
            BuildBinaryCache(json, binary);

        bool loaded = false;
        if (std::filesystem::exists(binary))
            loaded = sTypeRegistry.LoadBinary(binary, json, {});
        if (!loaded)
            loaded = sTypeRegistry.LoadFromFile(json);
        if (loaded)
            catalog = sTypeRegistry.GetCatalog();
        else if (command == "types")
        {
            std::cerr << fmt::format("the type dump {} cannot be read\n", *arguments->TypeDump);
            for (std::string const& problem : sTypeRegistry.GetErrors())
                std::cerr << "  " << problem << "\n";
            return Failure;
        }
    }
    if (world && catalog)
    {
        ObjectSchemaLoadResult const tables = sObjectSchemaMgr.LoadTables();
        WorldDatabase.Close();
        if (!tables.Loaded)
        {
            for (std::string const& problem : tables.Errors)
                std::cerr << fmt::format("client: {}\n", problem);
            return Failure;
        }
    }
    else if (world)
        WorldDatabase.Close();

    if (command == "core")
    {
        if (catalog == nullptr)
        {
            std::cerr << "client core needs a type dump; name one with --type-dump\n";
            return Failure;
        }
        return RunCore(*arguments, catalog);
    }

    if (command == "types")
    {
        if (catalog == nullptr)
        {
            std::cerr << "client types needs a type dump; name one with --type-dump\n";
            return Failure;
        }
        return RunTypes(*arguments, *catalog, arguments->Client ? LogConfig::Utf8Path(*arguments->Client) : std::filesystem::path());
    }

    if (!arguments->Client)
    {
        std::cerr << "client needs an install; name one with --client or AMBROSE_CLIENT_DIR\n";
        return Failure;
    }
    std::filesystem::path wad = LogConfig::Utf8Path(arguments->Wad);
    if (!wad.has_parent_path())
        wad = LogConfig::Utf8Path(*arguments->Client) / "Data" / "GameData" / wad;

    if (command == "lang")
    {
        std::filesystem::path const rootWad = LogConfig::Utf8Path(*arguments->Client) / "Data" / "GameData" / "Root.wad";
        std::string localeError;
        if (!sLocaleStore.Load(rootWad, arguments->Locale, localeError))
        {
            std::cerr << fmt::format("client lang cannot read the {} locale from {}: {}\n", arguments->Locale, ConfigMgr::PathToUtf8(rootWad), localeError);
            return Failure;
        }
        std::shared_ptr<LocaleTable const> const table = sLocaleStore.GetTable(arguments->Locale, &localeError);
        if (!table)
        {
            std::cerr << fmt::format("client lang has no {} locale: {}; this install holds {}\n", arguments->Locale, localeError, fmt::join(sLocaleStore.GetLocales(), ", "));
            return Failure;
        }
        if (arguments->List)
        {
            std::string const pattern = arguments->Subjects.empty() ? std::string() : arguments->Subjects.front();
            std::vector<std::string> const keys = table->FindKeys(pattern);
            for (std::string const& key : keys)
                if (std::string const* const text = table->Find(key))
                    std::cout << fmt::format("{}\t{}\n", key, *text);
            std::cerr << fmt::format("client: {} key(s) of the {} locale hold that text, out of {}\n", keys.size(), table->GetLocale(), table->GetKeyCount());
            return keys.empty() ? Failure : Success;
        }
        int status = Success;
        for (std::string const& key : arguments->Subjects)
        {
            if (std::string const* const text = table->Find(key))
                std::cout << *text << "\n";
            else
            {
                std::cerr << fmt::format("{}: the {} locale has no such key\n", key, table->GetLocale());
                status = Failure;
            }
        }
        return status;
    }

    std::string revision;
    if (std::optional<ClientInstall> const install = ClientInstall::Inspect(system, LogConfig::Utf8Path(*arguments->Client)))
        revision = install->Revision;

    if (command == "behaviors")
    {
        std::vector<BehaviorFactory> factories;
        if (!revision.empty())
            factories = ReadBehaviorCache(BehaviorCachePath(ClientLocator::GetDataFolder(system), revision));
        if (factories.empty())
        {
            std::filesystem::path const program = LogConfig::Utf8Path(*arguments->Client) / "Bin" / "WizardGraphicalClient.exe";
            std::unique_ptr<PeImage> const image = PeImage::Load(program, error);
            if (!image)
            {
                std::cerr << fmt::format("{}: {}\n", ConfigMgr::PathToUtf8(program), error);
                return Failure;
            }
            factories = BehaviorFactories::Find(*image, CodeIndex(*image));
            if (!revision.empty() && !factories.empty())
                WriteBehaviorCache(BehaviorCachePath(ClientLocator::GetDataFolder(system), revision), factories);
        }
        if (factories.empty())
        {
            std::cerr << "the client program registers no behavior factory this tool can find\n";
            return Failure;
        }
        return RunBehaviors(*arguments, factories, catalog.get());
    }

    std::vector<CachedMessage> messages;
    if ((command == "messages" || command == "handlers") && !revision.empty())
        messages = ReadMessageCache(MessageCachePath(ClientLocator::GetDataFolder(system), revision));
    if (command == "messages" && !messages.empty())
        return RunMessages(*arguments, messages);

    std::unique_ptr<KiwadArchive> archive;
    if (command != "handlers" || messages.empty())
    {
        std::filesystem::path const source = command == "handlers" ? LogConfig::Utf8Path(*arguments->Client) / "Data" / "GameData" / "Root.wad" : wad;
        archive = KiwadArchive::Open(source, error);
        if (!archive)
        {
            std::cerr << fmt::format("{}: {}\n", ConfigMgr::PathToUtf8(source), error);
            return Failure;
        }
    }
    if ((command == "messages" || command == "handlers") && messages.empty())
    {
        messages = GatherMessages(*archive);
        if (!revision.empty() && !messages.empty())
            WriteMessageCache(MessageCachePath(ClientLocator::GetDataFolder(system), revision), messages);
    }
    if (command == "messages")
        return RunMessages(*arguments, messages);

    if (command == "handlers")
    {
        std::vector<MessageHandlerRegistration> registrations;
        if (!revision.empty())
            registrations = ReadHandlerCache(HandlerCachePath(ClientLocator::GetDataFolder(system), revision));
        if (registrations.empty())
        {
            std::filesystem::path const program = LogConfig::Utf8Path(*arguments->Client) / "Bin" / "WizardGraphicalClient.exe";
            std::unique_ptr<PeImage> const image = PeImage::Load(program, error);
            if (!image)
            {
                std::cerr << fmt::format("{}: {}\n", ConfigMgr::PathToUtf8(program), error);
                return Failure;
            }
            registrations = MessageHandlers::Find(*image, CodeIndex(*image));
            if (!revision.empty() && !registrations.empty())
                WriteHandlerCache(HandlerCachePath(ClientLocator::GetDataFolder(system), revision), registrations);
        }
        if (registrations.empty())
        {
            std::cerr << "the client program registers no message handler this tool can find\n";
            return Failure;
        }
        return RunHandlers(*arguments, messages, registrations);
    }
    if (command == "template")
    {
        if (catalog == nullptr)
        {
            std::cerr << "client template needs a type dump; name one with --type-dump\n";
            return Failure;
        }
        return RunTemplate(*arguments, LogConfig::Utf8Path(*arguments->Client), *archive, catalog);
    }
    return RunWad(*arguments, *archive, catalog);
}
