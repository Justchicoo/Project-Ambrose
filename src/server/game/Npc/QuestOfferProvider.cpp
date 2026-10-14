/*
 * Project Ambrose by Imjustchico
 * Lists a quest giver's available quests as Prep options and the ones the wizard holds as Underway options, and passes the picked one on.
 */

#include "QuestOfferProvider.h"
#include "QuestOffers.h"

#include <utility>

QuestOfferProvider::QuestOfferProvider(std::shared_ptr<QuestStore const> quests, QuestLog const& log, std::shared_ptr<RequirementContext const> facts, OfferSink offer,
    UnderwaySink underway)
    : _quests(std::move(quests)), _log(log), _facts(std::move(facts)), _offer(std::move(offer)), _underway(std::move(underway))
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
        options.push_back({ std::string(QuestOffers::ServiceName), std::string(QuestOffers::PrepIcon), quest->Row.TitleKey, quest->Row.PrepKey, std::nullopt, std::nullopt });
    for (QuestInfo const* const quest : Underway(target.TemplateId))
    {
        QuestWireEncoder::UnderwayEntry underway;
        underway.UnderwayText = quest->Row.UnderwayKey;
        underway.QuestId = _log.Find(quest->Row.Name)->Gid;
        options.push_back({ std::string(QuestOffers::ServiceName), std::string(QuestOffers::UnderwayIcon), quest->Row.TitleKey, std::nullopt, std::nullopt, std::move(underway) });
    }
    return options;
}

void QuestOfferProvider::OnServiceInteraction(NpcServiceTarget const& target, uint32 index)
{
    std::vector<QuestInfo const*> const quests = Available(target.TemplateId);
    if (index < quests.size())
    {
        if (_offer)
            _offer(target, *quests[index]);
        return;
    }
    std::vector<QuestInfo const*> const underway = Underway(target.TemplateId);
    index -= static_cast<uint32>(quests.size());
    if (index < underway.size() && _underway)
        _underway(target, *underway[index], *_log.Find(underway[index]->Row.Name));
}

std::vector<QuestInfo const*> QuestOfferProvider::Available(uint32 templateId) const
{
    if (!_quests || !_facts)
        return {};
    return QuestOffers::Available(*_quests, templateId, _log, *_facts);
}

std::vector<QuestInfo const*> QuestOfferProvider::Underway(uint32 templateId) const
{
    if (!_quests)
        return {};
    return QuestOffers::Underway(*_quests, templateId, _log);
}
