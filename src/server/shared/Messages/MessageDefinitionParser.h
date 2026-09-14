/*
 * Project Ambrose by Imjustchico
 * Turns one client message XML document into a validated protocol with wire orders, reporting issues by line.
 */

#ifndef AMBROSE_MESSAGEDEFINITIONPARSER_H
#define AMBROSE_MESSAGEDEFINITIONPARSER_H

#include "MessageDefinition.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct MessageIssue
{
    std::string SourceFile;
    std::size_t Line = 0;
    std::string Message;

    std::string ToString() const;
};

struct MessageParseResult
{
    std::optional<ProtocolDef> Protocol;
    std::vector<MessageIssue> Errors;
    std::vector<MessageIssue> Warnings;

    bool Succeeded() const noexcept { return Errors.empty() && Protocol.has_value(); }
};

class MessageDefinitionParser
{
public:
    static constexpr std::size_t MaxMessagesPerProtocol = 255;

    static MessageParseResult Parse(std::string_view xml, std::string_view sourceFile);
    static bool IsMetadataName(std::string_view name) noexcept;
    static bool TagLess(std::string_view left, std::string_view right) noexcept;
};

#endif
