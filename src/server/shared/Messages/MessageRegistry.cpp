/*
 * Project Ambrose by Imjustchico
 * Loads message definitions from a set, an archive, or a client install, logs every issue, and builds the service and order index with parsed field defaults.
 */

#include "MessageRegistry.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"

#include <atomic>

std::size_t MessageDeclarationDetail::NextTypeIndex() noexcept
{
    static std::atomic<std::size_t> next{ 0 };
    return next.fetch_add(1, std::memory_order_relaxed);
}

MessageRegistry::MessageRegistry() = default;

MessageRegistry::~MessageRegistry() = default;

MessageRegistry& MessageRegistry::Instance()
{
    static MessageRegistry instance;
    return instance;
}

void MessageRegistry::Clear()
{
    _definitions.reset();
    _infos.clear();
    _index.clear();
    _bindings.clear();
    _errors.clear();
    _warnings.clear();
}

bool MessageRegistry::Load(MessageDefinitionSet definitions)
{
    Clear();
    _errors = definitions.GetErrors();
    _warnings = definitions.GetWarnings();
    if (definitions.HasErrors() || definitions.GetProtocols().empty())
    {
        if (_errors.empty())
            _errors.push_back({ "message definitions", 0, "no message protocols were loaded" });
        ReportLoadIssues();
        LOG_ERROR(LogFilter, "Message definitions failed to load with {} error(s)", _errors.size());
        return false;
    }

    auto owned = std::make_unique<MessageDefinitionSet>(std::move(definitions));
    std::vector<MessageInfo> infos;
    infos.reserve(owned->GetMessageCount());
    for (auto const& [serviceId, protocol] : owned->GetProtocols())
    {
        for (MessageDef const& message : protocol.Messages)
        {
            MessageInfo info;
            info.Protocol = &protocol;
            info.Definition = &message;
            info.Defaults.reserve(message.Fields.size());
            for (FieldDef const& field : message.Fields)
            {
                if (!field.DefaultValue)
                {
                    info.Defaults.push_back(Dml::DefaultValue(field.Type));
                    continue;
                }
                std::optional<DmlValue> parsed = Dml::ParseValue(field.Type, *field.DefaultValue);
                if (!parsed)
                {
                    _warnings.push_back({ protocol.SourceFile, field.Line, fmt::format("{}.{} has default '{}', which is not a valid {}; using the zero value", message.Tag, field.Name, *field.DefaultValue, Dml::GetTypeName(field.Type)) });
                    parsed = Dml::DefaultValue(field.Type);
                }
                info.Defaults.push_back(std::move(*parsed));
            }
            infos.push_back(std::move(info));
        }
    }

    std::vector<uint16> index(std::size_t{ 256 } * 256, 0);
    for (std::size_t i = 0; i < infos.size(); ++i)
        index[std::size_t{ infos[i].Protocol->ServiceId } * 256 + infos[i].Definition->Order] = static_cast<uint16>(i + 1);

    _definitions = std::move(owned);
    _infos = std::move(infos);
    _index = std::move(index);
    ReportLoadIssues();
    LOG_INFO(LogFilter, "Loaded {} message protocols with {} message ids and {} warning(s)", _definitions->GetProtocols().size(), _infos.size(), _warnings.size());
    return true;
}

bool MessageRegistry::LoadFromArchive(std::filesystem::path const& archivePath)
{
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(archivePath, error);
    if (!archive)
    {
        Clear();
        _errors.push_back({ ConfigMgr::PathToUtf8(archivePath), 0, error });
        ReportLoadIssues();
        return false;
    }
    MessageDefinitionSet definitions;
    definitions.LoadFromArchive(*archive);
    return Load(std::move(definitions));
}

bool MessageRegistry::LoadFromClient(std::filesystem::path const& clientDirectory)
{
    return LoadFromArchive(GetClientArchivePath(clientDirectory));
}

std::filesystem::path MessageRegistry::GetClientArchivePath(std::filesystem::path const& clientDirectory)
{
    return clientDirectory / "Data" / "GameData" / "Root.wad";
}

MessageInfo const* MessageRegistry::Find(uint8 serviceId, uint8 order) const noexcept
{
    if (_index.empty())
        return nullptr;
    uint16 const slot = _index[std::size_t{ serviceId } * 256 + order];
    return slot == 0 ? nullptr : &_infos[slot - 1];
}

MessageInfo const* MessageRegistry::Find(uint8 serviceId, std::string_view tag) const noexcept
{
    if (!_definitions)
        return nullptr;
    MessageDef const* const message = _definitions->FindByTag(serviceId, tag);
    return message ? Find(serviceId, message->Order) : nullptr;
}

void MessageRegistry::ReportLoadIssues() const
{
    for (MessageIssue const& warning : _warnings)
        LOG_WARN(LogFilter, "{}", warning.ToString());
    for (MessageIssue const& error : _errors)
        LOG_ERROR(LogFilter, "{}", error.ToString());
}

void MessageRegistry::ReportDeclarationError(std::string const& error) const
{
    LOG_ERROR(LogFilter, "Message declaration: {}", error);
}

void MessageRegistry::SkipField(ByteBuffer& buffer, DmlType type)
{
    switch (type)
    {
        case DmlType::Str:
            buffer.Skip(buffer.Read<uint16>());
            return;
        case DmlType::Wstr:
            buffer.Skip(std::size_t{ buffer.Read<uint16>() } * 2);
            return;
        default:
            buffer.Skip(Dml::GetFixedSize(type));
            return;
    }
}
