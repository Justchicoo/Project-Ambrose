/*
 * Project Ambrose by Imjustchico
 * What an NPC offers a wizard and how the offer is shown: the quests whose Prep dialog's first actor is the NPC's template that the wizard does not hold, has not completed unless the quest repeats, and whose requirements it meets, in the order of their title keys; and, for one of them, the Prep dialog as the client's ActorDialog, its start goals as a GoalCompilation and its rewards as a LootInfoList, each encoded for MSG_ACTORDIALOG and MSG_QUESTOFFER.
 */

#ifndef AMBROSE_QUESTOFFERS_H
#define AMBROSE_QUESTOFFERS_H

#include "ActorDialog.h"
#include "QuestLog.h"
#include "QuestStore.h"
#include "QuestWireEncoder.h"
#include "RequirementMgr.h"

#include <string>
#include <vector>

namespace QuestOffers
{
    inline constexpr std::string_view ServiceName = "QuestOfferService";
    inline constexpr std::string_view PrepIcon = "Prep";
    inline constexpr std::string_view CompletionType = "QuestInfo";

    struct Encoded
    {
        std::string Dialog;
        std::string Rewards;
        std::string GoalData;
        std::string Error;

        bool Ok() const noexcept { return Error.empty(); }
    };

    std::vector<QuestInfo const*> Available(QuestStore const& quests, uint32 templateId, QuestLog const& log, RequirementContext const& context);
    Dialogs::ActorDialog BuildPrepDialog(QuestInfo const& quest);
    QuestWireEncoder::GoalCompilation BuildStartGoals(QuestInfo const& quest);
    Encoded Encode(TypeCatalogPtr const& catalog, QuestInfo const& quest);
}

#endif
