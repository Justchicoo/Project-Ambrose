/*
 * Project Ambrose by Imjustchico
 * The errors every app has raised, gathered from each app's own grouping and kept in the panel's store so they outlive the app that raised them and the supervisor that gathered them. An app counts only what it has raised since it started, so a restart would lose the history if the count were taken as it comes: a group's total adds what earlier runs raised to what this run reports, and a run that went backwards is read as a restart rather than as an app that forgot. An operator can clear a group, which is remembered as the time it was cleared rather than a flag, so a group raised again after that is new again and says when it came back.
 */

#ifndef AMBROSE_PANELERRORS_H
#define AMBROSE_PANELERRORS_H

#include "Types.h"

#include <optional>
#include <string>
#include <vector>

class PanelStore;

struct PanelErrorGroup
{
    int64 Id = 0;
    std::string App;
    std::string Category;
    std::string File;
    uint32 Line = 0;
    std::string Function;
    std::string Template;
    std::string Level;
    std::string Revision;
    uint64 Count = 0;
    uint64 TotalCount = 0;
    int64 FirstEpochMs = 0;
    int64 LastEpochMs = 0;
    std::string LastMessage;
    std::optional<int64> ClearedEpochMs;

    bool IsNewSinceCleared() const noexcept { return ClearedEpochMs && LastEpochMs > *ClearedEpochMs; }
};

class PanelErrors
{
public:
    explicit PanelErrors(PanelStore& store);

    PanelErrors(PanelErrors const&) = delete;
    PanelErrors& operator=(PanelErrors const&) = delete;

    bool Record(std::string_view app, std::vector<PanelErrorGroup> const& reported, std::string& error);
    std::vector<PanelErrorGroup> List(std::string& error) const;
    bool Clear(int64 id, int64 whenEpochMs, std::string& error);
    bool Forget(std::string_view app, std::string& error);

private:
    PanelStore& _store;
};

#endif
