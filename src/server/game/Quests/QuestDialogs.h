/*
 * Project Ambrose by Imjustchico
 * The quest dialogs a wizard's client is playing, one per NPC and completion type, each with the quest it belongs to and the entry events its lines carry, so that MSG_COMPLETEDIALOG fires each of those events for QuestScript once and closes the dialog when its last event fires or the client reports it closed without one.
 */

#ifndef AMBROSE_QUESTDIALOGS_H
#define AMBROSE_QUESTDIALOGS_H

#include "ActorDialog.h"
#include "Types.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class QuestDialogs
{
public:
    static constexpr std::string_view QuestStart = "QuestStart";
    static constexpr std::string_view Underway = "QuestUnderway";
    static constexpr std::string_view QuestComplete = "QuestComplete";

    void Play(uint64 mobile, std::string_view completionType, std::string quest, Dialogs::ActorDialog const& dialog);
    std::optional<std::string> Complete(uint64 wizard, uint64 mobile, std::string_view completionType, std::string_view event);

private:
    struct Playing
    {
        std::string Quest;
        std::vector<std::string> Events;
    };

    std::map<std::pair<uint64, std::string>, Playing, std::less<>> _playing;
};

#endif
