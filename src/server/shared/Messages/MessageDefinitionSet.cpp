/*
 * Project Ambrose by Imjustchico
 * Adds protocols while rejecting duplicate ServiceIDs, out-of-order ids, and repeated tags, and reads every *Messages.xml entry out of a KIWAD archive.
 */

#include "MessageDefinitionSet.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <set>

bool MessageDefinitionSet::Add(std::string_view xml, std::string_view sourceFile)
{
    MessageParseResult result = MessageDefinitionParser::Parse(xml, sourceFile);
    _warnings.insert(_warnings.end(), result.Warnings.begin(), result.Warnings.end());
    _errors.insert(_errors.end(), result.Errors.begin(), result.Errors.end());
    if (!result.Succeeded())
        return false;
    return Add(std::move(*result.Protocol));
}

bool MessageDefinitionSet::Add(ProtocolDef protocol)
{
    auto const existing = _protocols.find(protocol.ServiceId);
    if (existing != _protocols.end())
    {
        _errors.push_back({ protocol.SourceFile, 0, fmt::format("ServiceID {} is already used by {}", protocol.ServiceId, existing->second.SourceFile) });
        return false;
    }
    std::set<std::string_view> tags;
    for (std::size_t i = 0; i < protocol.Messages.size(); ++i)
    {
        MessageDef const& message = protocol.Messages[i];
        if (message.Order == 0 || (i > 0 && message.Order <= protocol.Messages[i - 1].Order))
        {
            _errors.push_back({ protocol.SourceFile, message.Line, fmt::format("{} has order {}; orders must be 1-255 and increase through the protocol", message.Tag, message.Order) });
            return false;
        }
        if (!tags.insert(message.Tag).second)
        {
            _errors.push_back({ protocol.SourceFile, message.Line, fmt::format("{} appears more than once in ServiceID {}", message.Tag, protocol.ServiceId) });
            return false;
        }
    }
    uint8 const serviceId = protocol.ServiceId;
    _protocols.emplace(serviceId, std::move(protocol));
    return true;
}

bool MessageDefinitionSet::LoadFromArchive(KiwadArchive const& archive)
{
    std::vector<KiwadEntry const*> entries;
    for (KiwadEntry const& entry : archive.GetEntries())
        if (IsMessageFileName(entry.Name))
            entries.push_back(&entry);
    std::sort(entries.begin(), entries.end(), [](KiwadEntry const* left, KiwadEntry const* right) { return left->Name < right->Name; });

    std::string const archiveName = ConfigMgr::PathToUtf8(archive.GetPath().filename());
    if (entries.empty())
    {
        _errors.push_back({ archiveName, 0, "the archive has no message definition files" });
        return false;
    }

    bool succeeded = true;
    for (KiwadEntry const* entry : entries)
    {
        KiwadReadResult const read = archive.Read(*entry);
        if (!read.Succeeded())
        {
            _errors.push_back({ entry->Name, 0, fmt::format("could not read it from {}: {}", archiveName, read.Error) });
            succeeded = false;
            continue;
        }
        std::string_view const text(reinterpret_cast<char const*>(read.Data.data()), read.Data.size());
        if (text.substr(0, 4) == "BINd")
        {
            _errors.push_back({ entry->Name, 0, "it is a BINd container, not message XML text" });
            succeeded = false;
            continue;
        }
        if (!Add(text, entry->Name))
            succeeded = false;
    }
    return succeeded;
}

bool MessageDefinitionSet::IsMessageFileName(std::string_view entryName)
{
    std::string const name = Ambrose::ToLower(entryName);
    std::string_view rest(name);
    if (rest.size() < 4 || rest.substr(rest.size() - 4) != ".xml")
        return false;
    rest.remove_suffix(4);
    while (!rest.empty() && rest.back() >= '0' && rest.back() <= '9')
        rest.remove_suffix(1);
    std::string_view const suffix = "messages";
    return rest.size() >= suffix.size() && rest.substr(rest.size() - suffix.size()) == suffix;
}

ProtocolDef const* MessageDefinitionSet::FindService(uint8 serviceId) const noexcept
{
    auto const it = _protocols.find(serviceId);
    return it == _protocols.end() ? nullptr : &it->second;
}

MessageDef const* MessageDefinitionSet::Find(uint8 serviceId, uint8 order) const noexcept
{
    ProtocolDef const* const protocol = FindService(serviceId);
    return protocol ? protocol->FindByOrder(order) : nullptr;
}

MessageDef const* MessageDefinitionSet::FindByTag(uint8 serviceId, std::string_view tag) const noexcept
{
    ProtocolDef const* const protocol = FindService(serviceId);
    return protocol ? protocol->FindByTag(tag) : nullptr;
}

std::size_t MessageDefinitionSet::GetRecordCount() const noexcept
{
    std::size_t count = 0;
    for (auto const& [serviceId, protocol] : _protocols)
        count += protocol.RecordCount;
    return count;
}

std::size_t MessageDefinitionSet::GetMessageCount() const noexcept
{
    std::size_t count = 0;
    for (auto const& [serviceId, protocol] : _protocols)
        count += protocol.Messages.size();
    return count;
}

std::size_t MessageDefinitionSet::GetFieldCount() const noexcept
{
    std::size_t count = 0;
    for (auto const& [serviceId, protocol] : _protocols)
        for (MessageDef const& message : protocol.Messages)
            count += message.Fields.size();
    return count;
}

std::map<DmlType, std::size_t> MessageDefinitionSet::GetTypeCensus() const
{
    std::map<DmlType, std::size_t> census;
    for (auto const& [serviceId, protocol] : _protocols)
        for (MessageDef const& message : protocol.Messages)
            for (FieldDef const& field : message.Fields)
                ++census[field.Type];
    return census;
}
