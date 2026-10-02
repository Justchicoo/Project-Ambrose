/*
 * Project Ambrose by Imjustchico
 * The activity log read back out of the audit tables: a filter by event prefix, actor, subject, an operator acting or acted on, result, time range and address, newest first a page of at most 100 rows at a time behind a cursor, each row answered with its rendered sentence and its address only to those allowed to see it, CSV and JSON exports a spreadsheet cannot run a formula from, and the retention sweep that removes rows past their class's window inside one recorded transaction.
 */

#ifndef AMBROSE_PANELACTIVITY_H
#define AMBROSE_PANELACTIVITY_H

#include "PanelAudit.h"
#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;
struct AdminRequest;

struct PanelActivityFilter
{
    std::string Prefix;
    std::string ActorId;
    std::string Involving;
    std::string SubjectKind;
    std::string SubjectId;
    std::string Result;
    std::string Address;
    int64 FromEpochMs = 0;
    int64 ToEpochMs = 0;
    int64 Before = 0;
    uint32 Limit = 50;
};

struct PanelActivityPage
{
    std::vector<AuditEvent> Rows;
    int64 NextCursor = 0;
};

struct PanelActivitySweep
{
    int64 Security = 0;
    int64 HighVolume = 0;
    int64 Anchored = 0;
};

namespace PanelActivity
{
    inline constexpr uint32 DefaultPageRows = 50;
    inline constexpr uint32 MaxPageRows = 100;
    inline constexpr uint32 MaxExportRows = 10000;
    inline constexpr uint32 ExportCost = 30;
    inline constexpr int64 DefaultSecurityDays = 365;
    inline constexpr int64 DefaultHighVolumeDays = 90;
    inline constexpr int64 SweepIntervalMs = 3600000;

    std::optional<PanelActivityFilter> ParseFilter(AdminRequest const& request, std::string& field, std::string& error);
    std::string_view ResultName(std::string_view stored) noexcept;
    bool Read(PanelStore& store, PanelActivityFilter const& filter, PanelActivityPage& page, std::string& error);
    bool ShowsAddress(AuditEvent const& event, std::string_view viewerId, bool seesEveryAddress);
    nlohmann::json RowJson(AuditEvent const& event, bool showAddress);
    std::string CsvField(std::string_view value);
    std::string Csv(std::vector<AuditEvent> const& rows, std::function<bool(AuditEvent const&)> const& showAddress);
    bool Sweep(PanelStore& store, int64 nowEpochMs, int64 securityDays, int64 highVolumeDays, PanelActivitySweep& removed, std::string& error);
}

#endif
