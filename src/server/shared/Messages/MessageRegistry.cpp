/*
 * Project Ambrose by Imjustchico
 * Builds catalogs from definition sets, archives, or a client install, re-resolves every declaration, and publishes a catalog only when all of it is valid.
 */

#include "MessageRegistry.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"

#include <atomic>
#include <functional>

std::size_t MessageDeclarationDetail::NextTypeIndex() noexcept
{
    static std::atomic<std::size_t> next{ 0 };
    return next.fetch_add(1, std::memory_order_relaxed);
}

MessageCatalog::MessageCatalog(std::shared_ptr<Tables const> tables, std::vector<MessageBinding> bindings, uint64 generation)
    : _tables(std::move(tables)), _bindings(std::move(bindings)), _generation(generation)
{
}

bool MessageCatalog::Contains(MessageInfo const& info) const noexcept
{
    std::vector<MessageInfo> const& infos = _tables->Infos;
    return !infos.empty() && !std::less<MessageInfo const*>()(&info, infos.data()) && std::less<MessageInfo const*>()(&info, infos.data() + infos.size());
}

MessageInfo const* MessageCatalog::Find(uint8 serviceId, uint8 order) const noexcept
{
    uint16 const slot = _tables->Index[std::size_t{ serviceId } * 256 + order];
    return slot == 0 ? nullptr : &_tables->Infos[slot - 1];
}

MessageInfo const* MessageCatalog::Find(uint8 serviceId, std::string_view tag) const noexcept
{
    MessageDef const* const message = _tables->Definitions->FindByTag(serviceId, tag);
    return message ? Find(serviceId, message->Order) : nullptr;
}

void MessageCatalog::SkipField(ByteBuffer& buffer, DmlType type)
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

MessageRegistry::MessageRegistry() = default;

MessageRegistry::~MessageRegistry() = default;

MessageRegistry& MessageRegistry::Instance()
{
    static MessageRegistry instance;
    return instance;
}

MessageCatalogPtr MessageRegistry::GetCatalog() const
{
    return _catalog.load();
}

bool MessageRegistry::IsLoaded() const
{
    return GetCatalog() != nullptr;
}

uint64 MessageRegistry::GetGeneration() const
{
    MessageCatalogPtr const catalog = GetCatalog();
    return catalog ? catalog->GetGeneration() : 0;
}

std::vector<MessageIssue> MessageRegistry::GetErrors() const
{
    std::lock_guard<std::mutex> lock(_writeMutex);
    return _errors;
}

std::vector<MessageIssue> MessageRegistry::GetWarnings() const
{
    std::lock_guard<std::mutex> lock(_writeMutex);
    return _warnings;
}

MessageCatalogPtr MessageRegistry::GetLoadedCatalog() const
{
    MessageCatalogPtr catalog = GetCatalog();
    if (!catalog)
        throw std::logic_error("message definitions are not loaded");
    return catalog;
}

void MessageRegistry::Publish(MessageCatalogPtr catalog)
{
    _catalog.store(std::move(catalog));
}

void MessageRegistry::Clear()
{
    std::lock_guard<std::mutex> lock(_writeMutex);
    Publish(nullptr);
    _declarations.clear();
    _errors.clear();
    _warnings.clear();
}

bool MessageRegistry::Load(MessageDefinitionSet definitions)
{
    std::lock_guard<std::mutex> lock(_writeMutex);
    std::vector<MessageIssue> errors = definitions.GetErrors();
    std::vector<MessageIssue> warnings = definitions.GetWarnings();
    MessageCatalogPtr const previous = GetCatalog();

    auto fail = [&]()
    {
        _errors = std::move(errors);
        _warnings = std::move(warnings);
        ReportLoadIssues(_errors, _warnings);
        LOG_ERROR(LogFilter, "Message definitions failed to load with {} error(s); {}", _errors.size(), previous ? fmt::format("generation {} stays active", previous->GetGeneration()) : std::string("nothing is loaded"));
        return false;
    };

    if (!errors.empty() || definitions.GetProtocols().empty())
    {
        if (errors.empty())
            errors.push_back({ "message definitions", 0, "no message protocols were loaded" });
        return fail();
    }

    auto tables = std::make_shared<MessageCatalog::Tables>();
    tables->Definitions = std::make_unique<MessageDefinitionSet>(std::move(definitions));
    tables->Infos.reserve(tables->Definitions->GetMessageCount());
    for (auto const& [serviceId, protocol] : tables->Definitions->GetProtocols())
    {
        for (MessageDef const& message : protocol.Messages)
        {
            MessageInfo info;
            info.Protocol = &protocol;
            info.Definition = &message;
            info.Defaults.reserve(message.Fields.size());
            for (FieldDef const& field : message.Fields)
            {
                std::size_t const fixedSize = Dml::GetFixedSize(field.Type);
                info.MinSize += fixedSize != 0 ? fixedSize : 2;
                if (!field.DefaultValue)
                {
                    info.Defaults.push_back(Dml::DefaultValue(field.Type));
                    continue;
                }
                std::optional<DmlValue> parsed = Dml::ParseValue(field.Type, *field.DefaultValue);
                if (!parsed)
                {
                    warnings.push_back({ protocol.SourceFile, field.Line, fmt::format("{}.{} has default '{}', which is not a valid {}; using the zero value", message.Tag, field.Name, *field.DefaultValue, Dml::GetTypeName(field.Type)) });
                    parsed = Dml::DefaultValue(field.Type);
                }
                info.Defaults.push_back(std::move(*parsed));
            }
            tables->Infos.push_back(std::move(info));
        }
    }
    tables->Index.assign(std::size_t{ 256 } * 256, 0);
    for (std::size_t i = 0; i < tables->Infos.size(); ++i)
        tables->Index[std::size_t{ tables->Infos[i].Protocol->ServiceId } * 256 + tables->Infos[i].Definition->Order] = static_cast<uint16>(i + 1);
    tables->Warnings = warnings;

    MessageCatalog const unbound(tables, {}, 0);
    std::vector<MessageBinding> bindings;
    std::vector<std::string> declarationErrors;
    for (Declaration const& declaration : _declarations)
    {
        if (bindings.size() <= declaration.TypeIndex)
            bindings.resize(declaration.TypeIndex + 1);
        declaration.Resolve(unbound, bindings[declaration.TypeIndex], declarationErrors);
    }
    if (!declarationErrors.empty())
    {
        for (std::string& error : declarationErrors)
            errors.push_back({ "message declarations", 0, std::move(error) });
        return fail();
    }

    uint64 const generation = _nextGeneration++;
    auto catalog = std::shared_ptr<MessageCatalog const>(new MessageCatalog(std::move(tables), std::move(bindings), generation));
    std::size_t const protocols = catalog->GetDefinitions().GetProtocols().size();
    std::size_t const messages = catalog->GetMessageCount();
    Publish(std::move(catalog));
    _errors.clear();
    _warnings = std::move(warnings);
    ReportLoadIssues(_errors, _warnings);
    LOG_INFO(LogFilter, "Loaded message definitions generation {}: {} protocols, {} message ids, {} declarations, {} warning(s)", generation, protocols, messages, _declarations.size(), _warnings.size());
    return true;
}

bool MessageRegistry::LoadFromArchive(std::filesystem::path const& archivePath)
{
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(archivePath, error);
    if (!archive)
    {
        std::lock_guard<std::mutex> lock(_writeMutex);
        _errors = { { ConfigMgr::PathToUtf8(archivePath), 0, error } };
        _warnings.clear();
        ReportLoadIssues(_errors, _warnings);
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

bool MessageRegistry::DeclareOne(std::size_t typeIndex, Resolver resolve, std::vector<std::string>& errors)
{
    std::lock_guard<std::mutex> lock(_writeMutex);
    bool const known = std::any_of(_declarations.begin(), _declarations.end(), [typeIndex](Declaration const& declaration) { return declaration.TypeIndex == typeIndex; });
    MessageCatalogPtr const current = GetCatalog();
    if (!current)
    {
        if (!known)
            _declarations.push_back({ typeIndex, resolve });
        return true;
    }

    MessageBinding binding;
    std::vector<std::string> resolveErrors;
    if (!resolve(*current, binding, resolveErrors))
    {
        for (std::string const& error : resolveErrors)
            LOG_ERROR(LogFilter, "Message declaration: {}", error);
        errors.insert(errors.end(), resolveErrors.begin(), resolveErrors.end());
        return false;
    }
    if (!known)
        _declarations.push_back({ typeIndex, resolve });
    std::vector<MessageBinding> bindings = current->_bindings;
    if (bindings.size() <= typeIndex)
        bindings.resize(typeIndex + 1);
    bindings[typeIndex] = std::move(binding);
    Publish(std::shared_ptr<MessageCatalog const>(new MessageCatalog(current->_tables, std::move(bindings), current->GetGeneration())));
    return true;
}

MessageInfoPtr MessageRegistry::Find(uint8 serviceId, uint8 order) const
{
    MessageCatalogPtr catalog = GetCatalog();
    MessageInfo const* const info = catalog ? catalog->Find(serviceId, order) : nullptr;
    return info ? MessageInfoPtr(std::move(catalog), info) : nullptr;
}

MessageInfoPtr MessageRegistry::Find(uint8 serviceId, std::string_view tag) const
{
    MessageCatalogPtr catalog = GetCatalog();
    MessageInfo const* const info = catalog ? catalog->Find(serviceId, tag) : nullptr;
    return info ? MessageInfoPtr(std::move(catalog), info) : nullptr;
}

std::size_t MessageRegistry::GetMessageCount() const
{
    MessageCatalogPtr const catalog = GetCatalog();
    return catalog ? catalog->GetMessageCount() : 0;
}

void MessageRegistry::ReportLoadIssues(std::vector<MessageIssue> const& errors, std::vector<MessageIssue> const& warnings) const
{
    for (MessageIssue const& warning : warnings)
        LOG_WARN(LogFilter, "{}", warning.ToString());
    for (MessageIssue const& error : errors)
        LOG_ERROR(LogFilter, "{}", error.ToString());
}
