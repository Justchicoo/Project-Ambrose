/*
 * Project Ambrose by Imjustchico
 * The actor dialog tag, NPC dialog entries, dialog events and indexed madlib blocks held by a client-facing actor dialog.
 */

#ifndef AMBROSE_ACTORDIALOG_H
#define AMBROSE_ACTORDIALOG_H

#include "../Quests/QuestMadlibs.h"

#include <optional>
#include <string>
#include <vector>

namespace Dialogs
{
    struct ActorDialogEntry
    {
        uint32 ActorTemplateId = 0;
        std::string DialogKey;
        std::string Picture;
        std::string GuiDisplay;
        std::string Sound;
        std::string Action;
        std::string DialogEvent;
        std::vector<std::string> Animations;
        std::optional<std::string> PersonaName;
        std::optional<std::string> NameOverride;
    };

    struct ActorMadlib
    {
        QuestMadlibs::Block Block;
        uint32 Index = 0;
    };

    struct ActorDialog
    {
        std::string Tag;
        std::vector<ActorDialogEntry> Entries;
        std::vector<ActorMadlib> Madlibs;
        std::vector<std::string> DialogEvents;
        bool NoAggroWhileDialogIsUp = false;
        bool NoAggroNoDelay = false;
    };
}

#endif
