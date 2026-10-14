/*
 * Project Ambrose by Imjustchico
 * Keeps the quest dialogs a wizard's client is playing and fires each entry event a played line carries for QuestScript once.
 */

#include "QuestDialogs.h"
#include "ScriptMgr.h"

#include <algorithm>

void QuestDialogs::Play(uint64 mobile, std::string_view completionType, std::string quest, Dialogs::ActorDialog const& dialog)
{
    Playing playing{ std::move(quest), {} };
    for (Dialogs::ActorDialogEntry const& entry : dialog.Entries)
        if (!entry.DialogEvent.empty() && std::ranges::find(playing.Events, entry.DialogEvent) == playing.Events.end())
            playing.Events.push_back(entry.DialogEvent);
    _playing.insert_or_assign({ mobile, std::string(completionType) }, std::move(playing));
}

std::optional<std::string> QuestDialogs::Complete(uint64 wizard, uint64 mobile, std::string_view completionType, std::string_view event)
{
    auto const found = _playing.find(std::pair{ mobile, std::string(completionType) });
    if (found == _playing.end())
        return std::nullopt;
    if (event.empty())
    {
        _playing.erase(found);
        return std::nullopt;
    }
    auto const pending = std::ranges::find(found->second.Events, event);
    if (pending == found->second.Events.end())
        return std::nullopt;
    found->second.Events.erase(pending);
    std::string const quest = found->second.Quest;
    if (found->second.Events.empty())
        _playing.erase(found);
    sScriptMgr.OnDialogEvent(wizard, quest, event);
    return quest;
}
