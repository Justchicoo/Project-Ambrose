/*
 * Project Ambrose by Imjustchico
 * Merging what an app reports into what the panel already knew about it. A group is found by the place it was raised at rather than by an identifier the app chose, because an app that restarts chooses new ones. The count an app reports is for the life of that app, so the total is what earlier runs left plus what this run says, and a count that went backwards means the app restarted rather than that it lost errors.
 */

#include "PanelErrors.h"

#include "PanelStore.h"

PanelErrors::PanelErrors(PanelStore& store) : _store(store)
{
}

bool PanelErrors::Record(std::string_view app, std::vector<PanelErrorGroup> const& reported, std::string& error)
{
    for (PanelErrorGroup const& group : reported)
    {
        std::optional<PanelStore::Statement> found = _store.Prepare(
            "SELECT id, count, total_count, first_epoch_ms FROM panel_error_group WHERE app = ?1 AND category = ?2 AND file = ?3 AND line = ?4 AND template = ?5", error);
        if (!found)
            return false;
        found->Bind(1, app);
        found->Bind(2, group.Category);
        found->Bind(3, group.File);
        found->Bind(4, static_cast<int64>(group.Line));
        found->Bind(5, group.Template);
        bool const exists = found->Step(error);
        if (!exists && !error.empty())
            return false;

        if (exists)
        {
            int64 const id = found->Int64(0);
            uint64 const seen = static_cast<uint64>(found->Int64(1));
            uint64 const total = static_cast<uint64>(found->Int64(2));
            int64 const first = found->Int64(3);
            uint64 const carried = group.Count >= seen ? total + (group.Count - seen) : total + group.Count;
            found.reset();

            std::optional<PanelStore::Statement> update = _store.Prepare(
                "UPDATE panel_error_group SET function = ?1, level = ?2, revision = ?3, count = ?4, total_count = ?5, first_epoch_ms = ?6, last_epoch_ms = ?7, last_message = ?8 WHERE id = ?9", error);
            if (!update)
                return false;
            update->Bind(1, group.Function);
            update->Bind(2, group.Level);
            update->Bind(3, group.Revision);
            update->Bind(4, static_cast<int64>(group.Count));
            update->Bind(5, static_cast<int64>(carried));
            update->Bind(6, first < group.FirstEpochMs && first != 0 ? first : group.FirstEpochMs);
            update->Bind(7, group.LastEpochMs);
            update->Bind(8, group.LastMessage);
            update->Bind(9, id);
            if (!update->Run(error))
                return false;
            continue;
        }
        found.reset();

        std::optional<PanelStore::Statement> insert = _store.Prepare(
            "INSERT INTO panel_error_group (app, category, file, line, function, template, level, revision, count, total_count, first_epoch_ms, last_epoch_ms, last_message)"
            " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13)", error);
        if (!insert)
            return false;
        insert->Bind(1, app);
        insert->Bind(2, group.Category);
        insert->Bind(3, group.File);
        insert->Bind(4, static_cast<int64>(group.Line));
        insert->Bind(5, group.Function);
        insert->Bind(6, group.Template);
        insert->Bind(7, group.Level);
        insert->Bind(8, group.Revision);
        insert->Bind(9, static_cast<int64>(group.Count));
        insert->Bind(10, static_cast<int64>(group.Count));
        insert->Bind(11, group.FirstEpochMs);
        insert->Bind(12, group.LastEpochMs);
        insert->Bind(13, group.LastMessage);
        if (!insert->Run(error))
            return false;
    }
    return true;
}

std::vector<PanelErrorGroup> PanelErrors::List(std::string& error) const
{
    std::vector<PanelErrorGroup> out;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT id, app, category, file, line, function, template, level, revision, count, total_count, first_epoch_ms, last_epoch_ms, last_message, cleared_epoch_ms"
        " FROM panel_error_group ORDER BY last_epoch_ms DESC, total_count DESC", error);
    if (!rows)
        return out;
    while (rows->Step(error))
    {
        PanelErrorGroup group;
        group.Id = rows->Int64(0);
        group.App = rows->Text(1);
        group.Category = rows->Text(2);
        group.File = rows->Text(3);
        group.Line = static_cast<uint32>(rows->Int64(4));
        group.Function = rows->Text(5);
        group.Template = rows->Text(6);
        group.Level = rows->Text(7);
        group.Revision = rows->Text(8);
        group.Count = static_cast<uint64>(rows->Int64(9));
        group.TotalCount = static_cast<uint64>(rows->Int64(10));
        group.FirstEpochMs = rows->Int64(11);
        group.LastEpochMs = rows->Int64(12);
        group.LastMessage = rows->Text(13);
        if (!rows->IsNull(14))
            group.ClearedEpochMs = rows->Int64(14);
        out.push_back(std::move(group));
    }
    if (!error.empty())
        out.clear();
    return out;
}

bool PanelErrors::Clear(int64 id, int64 whenEpochMs, std::string& error)
{
    std::optional<PanelStore::Statement> update = _store.Prepare("UPDATE panel_error_group SET cleared_epoch_ms = ?1 WHERE id = ?2", error);
    if (!update)
        return false;
    update->Bind(1, whenEpochMs);
    update->Bind(2, id);
    return update->Run(error);
}

bool PanelErrors::Forget(std::string_view app, std::string& error)
{
    std::optional<PanelStore::Statement> remove = _store.Prepare("DELETE FROM panel_error_group WHERE app = ?1", error);
    if (!remove)
        return false;
    remove->Bind(1, app);
    return remove->Run(error);
}
