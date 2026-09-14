/*
 * Project Ambrose by Imjustchico
 * Every protocol from the client's message files, keyed by ServiceID, with archive loading, lookups, and a field type census.
 */

#ifndef AMBROSE_MESSAGEDEFINITIONSET_H
#define AMBROSE_MESSAGEDEFINITIONSET_H

#include "MessageDefinitionParser.h"

#include <cstddef>
#include <map>
#include <string_view>
#include <vector>

class KiwadArchive;

class MessageDefinitionSet
{
public:
    bool Add(std::string_view xml, std::string_view sourceFile);
    bool Add(ProtocolDef protocol);
    bool LoadFromArchive(KiwadArchive const& archive);

    static bool IsMessageFileName(std::string_view entryName);

    ProtocolDef const* FindService(uint8 serviceId) const noexcept;
    MessageDef const* Find(uint8 serviceId, uint8 order) const noexcept;
    MessageDef const* FindByTag(uint8 serviceId, std::string_view tag) const noexcept;

    std::map<uint8, ProtocolDef> const& GetProtocols() const noexcept { return _protocols; }
    std::size_t GetRecordCount() const noexcept;
    std::size_t GetMessageCount() const noexcept;
    std::size_t GetFieldCount() const noexcept;
    std::map<DmlType, std::size_t> GetTypeCensus() const;

    std::vector<MessageIssue> const& GetErrors() const noexcept { return _errors; }
    std::vector<MessageIssue> const& GetWarnings() const noexcept { return _warnings; }
    bool HasErrors() const noexcept { return !_errors.empty(); }

private:
    std::map<uint8, ProtocolDef> _protocols;
    std::vector<MessageIssue> _errors;
    std::vector<MessageIssue> _warnings;
};

#endif
