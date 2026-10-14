/*
 * Project Ambrose by Imjustchico
 * The NPC service provider of quest givers: an NPC whose template is the first actor of a quest's Prep dialog offers the wizard each quest it can take there, judged by the facts the session knows of it, its school among them, as a Prep option titled by the quest's title key, then each of its quests the wizard holds as an Underway option carrying the quest's underway text, ahead of any script's options; picking an offer hands that quest to the session to offer, and picking an underway quest hands it over to be reminded of.
 */

#ifndef AMBROSE_QUESTOFFERPROVIDER_H
#define AMBROSE_QUESTOFFERPROVIDER_H

#include "NpcServiceProvider.h"
#include "QuestLog.h"
#include "QuestStore.h"
#include "RequirementMgr.h"

#include <functional>
#include <memory>

class QuestOfferProvider : public NpcServiceProvider
{
public:
    using OfferSink = std::function<void(NpcServiceTarget const& target, QuestInfo const& quest)>;
    using UnderwaySink = std::function<void(NpcServiceTarget const& target, QuestInfo const& quest, QuestLogQuest const& held)>;

    static constexpr int32 Priority = 100;

    QuestOfferProvider(std::shared_ptr<QuestStore const> quests, QuestLog const& log, std::shared_ptr<RequirementContext const> facts, OfferSink offer, UnderwaySink underway);

    static bool Serves(QuestStore const& quests, uint32 templateId);

    std::string_view GetName() const override { return "quest_offer"; }
    int32 GetPriority() const override { return Priority; }
    std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const override;
    void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) override;

private:
    std::vector<QuestInfo const*> Available(uint32 templateId) const;
    std::vector<QuestInfo const*> Underway(uint32 templateId) const;

    std::shared_ptr<QuestStore const> _quests;
    QuestLog const& _log;
    std::shared_ptr<RequirementContext const> _facts;
    OfferSink _offer;
    UnderwaySink _underway;
};

#endif
