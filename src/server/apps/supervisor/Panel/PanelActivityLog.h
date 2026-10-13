/*
 * Project Ambrose by Imjustchico
 * The activity log read side: filtered queries with cursor pagination, IP redaction for readers without the grant, CSV and JSON export with formula-injection protection, and the hourly retention sweep that keeps security rows longer and records itself.
 */

#ifndef AMBROSE_PANELACTIVITYLOG_H
#define AMBROSE_PANELACTIVITYLOG_H

#include "Types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;
struct AuditEvent;

namespace PanelActivityLog
{
    constexpr int64 MaxRows = 100;
    constexpr std::string_view SweepEventName = "activity:sweep.ran";
    constexpr std::string_view ExportEventName = "activity:export.created";

    struct Filter
    {
        std::string EventPrefix;
        std::string Actor;
        std::string Subject;
        std::string Result;
        int64 FromEpochMs = 0;
        int64 ToEpochMs = 0;
        std::string Address;
        int64 Cursor = 0;
        int64 Limit = MaxRows;
    };

    struct Row
    {
        int64 Id = 0;
        std::string EventId;
        int64 CreatedEpochMs = 0;
        std::string Name;
        std::string Sentence;
        std::string ActorType;
        std::string ActorId;
        std::string ActorName;
        std::string Address;
        std::string Node;
        std::string Result;
        std::string Error;
        std::string Reason;
        std::string ApiKeyId;
    };

    struct Page
    {
        std::vector<Row> Rows;
        int64 NextCursor = 0;
        bool HasMore = false;
    };

    bool Query(PanelStore& store, Filter const& filter, bool canSeeIps, std::string_view selfActorId, Page& page, std::string& error);
    bool Details(PanelStore& store, int64 id, bool canSeeIps, std::string_view selfActorId, Row& row, std::string& properties, std::string& error);

    std::string ToCsv(std::vector<Row> const& rows);
    std::string ToJson(std::vector<Row> const& rows);
    std::string CsvCell(std::string_view value);

    bool IsSecurityEvent(std::string_view name) noexcept;
    int64 Sweep(PanelStore& store, int64 securityRetentionDays, int64 highVolumeRetentionDays, std::string& error);
}

#endif
