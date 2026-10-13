/*
 * Project Ambrose by Imjustchico
 * The NPC service provider of quest givers: an NPC whose template is the first actor of a quest's Prep dialog offers the wizard each quest it can take there, as a Prep option titled by the quest's title key, ahead of any script's options, and picking one hands that quest to the session to offer.
 */

#ifndef AMBROSE_QUESTOFFERPROVIDER_H
#define AMBROSE_QUESTOFFERPROVIDER_H

#include "NpcServiceProvider.h"
#include "QuestLog.h"
#include "QuestStore.h"

#include <functional>
#include <memory>

class QuestOfferProvider : public NpcServiceProvider
{
public:
    using OfferSink = std::function<void(NpcServiceTarget const& target, QuestInfo const& quest)>;

    static constexpr int32 Priority = 100;

    QuestOfferProvider(std::shared_ptr<QuestStore const> quests, QuestLog const& log, OfferSink offer);

    static bool Serves(QuestStore const& quests, uint32 templateId);

    std::string_view GetName() const override { return "quest_offer"; }
    int32 GetPriority() const override { return Priority; }
    NpcWizBang::Id GetWizBang(NpcServiceTarget const& target) const override;
    std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const override;
    void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) override;

private:
    std::vector<QuestInfo const*> Available(uint32 templateId) const;

    std::shared_ptr<QuestStore const> _quests;
    QuestLog const& _log;
    OfferSink _offer;
};

#endif
