/*
 * Project Ambrose by Imjustchico
 * Lists a quest giver's available quests as Prep options and passes the picked one on.
 */

#include "QuestOfferProvider.h"
#include "QuestLogContext.h"
#include "QuestOffers.h"

#include <utility>

QuestOfferProvider::QuestOfferProvider(std::shared_ptr<QuestStore const> quests, QuestLog const& log, OfferSink offer)
    : _quests(std::move(quests)), _log(log), _offer(std::move(offer))
{
}

bool QuestOfferProvider::Serves(QuestStore const& quests, uint32 templateId)
{
    return !quests.GetQuestsOfferedBy(templateId).empty();
}

std::vector<NpcServiceOption> QuestOfferProvider::GetServiceOptions(NpcServiceTarget const& target) const
{
    std::vector<NpcServiceOption> options;
    for (QuestInfo const* const quest : Available(target.TemplateId))
        options.push_back({ std::string(QuestOffers::ServiceName), std::string(QuestOffers::PrepIcon), quest->Row.TitleKey, quest->Row.PrepKey, std::nullopt });
    return options;
}

void QuestOfferProvider::OnServiceInteraction(NpcServiceTarget const& target, uint32 index)
{
    std::vector<QuestInfo const*> const quests = Available(target.TemplateId);
    if (index < quests.size() && _offer)
        _offer(target, *quests[index]);
}

std::vector<QuestInfo const*> QuestOfferProvider::Available(uint32 templateId) const
{
    if (!_quests)
        return {};
    QuestLogContext const context(_log);
    return QuestOffers::Available(*_quests, templateId, _log, context);
}
