/*
 * Project Ambrose by Imjustchico
 * The optional text log of every DML message a session sends or receives, each line giving the session, which way it went, the protocol and name the running definitions give it, its service and order and its fields; Network.PacketLog.Enable, .Filter and .Suppress are read at each message, so a change applies from the next one, a filter keeps only the messages it names, and a suppressed one is left out; the fields of a message that carries credentials are never written.
 */

#ifndef AMBROSE_PACKETLOG_H
#define AMBROSE_PACKETLOG_H

#include "MessageRegistry.h"
#include "Types.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PacketLog
{
    enum class Direction : uint8
    {
        ClientToServer,
        ServerToClient
    };

    struct Options
    {
        bool Enabled = false;
        std::vector<std::string> Filter;
        std::vector<std::string> Suppress;

        static Options FromSettings();
        static std::vector<std::string> ParseNames(std::string_view list);
        bool Wants(std::string_view tag) const;
    };

    bool IsRedacted(std::string_view tag) noexcept;
    std::string Format(Direction direction, uint16 sessionId, MessageCatalogPtr const& catalog, uint8 serviceId, uint8 order, std::span<uint8 const> body);
    void Record(Direction direction, uint16 sessionId, uint8 serviceId, uint8 order, std::span<uint8 const> body);
}

#endif
