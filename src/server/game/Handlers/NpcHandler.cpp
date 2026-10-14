/*
 * Project Ambrose by Imjustchico
 * The NPC service menu: after each move the NPCs a wizard sees, objects whose template carries NPCBehavior, are checked against their service range, only an NPC some provider gives an option counting, as only a quest, a shop or a script's option earns a prompt, and a wizard entering one is sent MSG_SENDNPCOPTIONS with the NPC's global id, as the client finds an NPC by it, and the ServiceMementoBase of its menu, the options every NpcScript serving its template offers, and one leaving is sent MSG_LEAVESERVICERANGE, once per crossing. MSG_INTERACTNPC, a click on a menu, is routed by its flat service index to the provider that owns that option, rebuilt as it was offered with its providers, which the menu's entries point at and so live as long as it does; an empty service name closes a shop, and an NPC the wizard is not in range of, or an index past its menu, is logged and ignored. MSG_INTERACTOPTION on an NPC the wizard is in range of is how the client answers its prompt, its options being InteractableOptions, and is routed the same way by option index; on any other object it, and MSG_INTERACTOBJECT, are logged until a provider offers interactables. MSG_ACCEPTQUEST for an offer that stands gives the wizard the quest with its start goals, which its quest book is then shown. An NPC a talk-to goal names lists the goal first; picking it plays the goal's Completion dialog and completes it, and when that completes the quest the NPC's menu is sent again with Reinteract 2 so its next quest can be offered. MSG_INTERACTCOMPLETEGOAL asks for that dialog and MSG_COMPLETEPERSONA completes the goal, each only for an active persona goal the wizard holds that names this NPC, in range. Every quest dialog shows only the lines whose requirements the wizard meets: accepting plays the quest's Start dialog, completing it by talking plays its Complete dialog, and a giver lists each quest it gave that the wizard holds, picking which, or MSG_INTERACTUNDERWAYQUEST, plays the quest's Underway dialog. MSG_COMPLETEDIALOG fires an entry event of a dialog the wizard is being shown once, for QuestScript and into the zone's triggers, and MSG_REQUESTQUESTDIALOG answers a held quest's name id with its Start dialog, or its Prep dialog when it has none, in MSG_QUESTDIALOG.
 */

#include "GameSession.h"
#include "Log.h"
#include "MapMgr.h"
#include "NpcServiceMemento.h"
#include "ObjectTemplateMgr.h"
#include "ObjectViews.h"
#include "QuestMgr.h"
#include "PersonaGoalProvider.h"
#include "QuestBook.h"
#include "QuestOfferProvider.h"
#include "QuestOffers.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace
{
    constexpr std::string_view NpcBehavior = "NPCBehavior";

    std::vector<std::unique_ptr<NpcServiceProvider>> ProvidersFor(uint32 templateId)
    {
        std::vector<std::unique_ptr<NpcServiceProvider>> providers;
        for (NpcScript* script : sScriptMgr.GetNpcScripts(templateId))
            providers.push_back(std::make_unique<ScriptNpcProvider>(*script));
        return providers;
    }

    struct ServedMenu
    {
        std::vector<std::unique_ptr<NpcServiceProvider>> Providers;
        NpcServiceMenu Menu;
    };

    ServedMenu MenuFor(NpcServiceTarget const& target, std::vector<std::unique_ptr<NpcServiceProvider>> quests)
    {
        ServedMenu served;
        served.Providers = ProvidersFor(target.TemplateId);
        for (std::unique_ptr<NpcServiceProvider>& provider : quests)
            served.Providers.push_back(std::move(provider));
        std::vector<NpcServiceProvider*> pointers;
        for (std::unique_ptr<NpcServiceProvider> const& provider : served.Providers)
            pointers.push_back(provider.get());
        served.Menu = NpcServiceMenu::Build(pointers, target);
        return served;
    }
}

bool GameSession::IsNpcTemplate(uint32 templateId)
{
    auto const known = _npcTemplates.find(templateId);
    if (known != _npcTemplates.end())
        return known->second;
    std::shared_ptr<ObjectTemplate const> const found = sObjectTemplateMgr.GetTemplate(templateId);
    bool const npc = found && std::find(found->Behaviors.begin(), found->Behaviors.end(), NpcBehavior) != found->Behaviors.end();
    _npcTemplates.emplace(templateId, npc);
    return npc;
}

void GameSession::CheckNpcServices()
{
    if (!_mapId)
        return;
    Map const* const map = sMapMgr.Find(*_mapId);
    if (!map)
        return;
    float const radius = NpcServiceRange::ResolveRadius(std::nullopt);
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    std::vector<NpcServiceRange::Npc> npcs;
    for (MapObject const& object : map->GetObjects())
    {
        uint32 const templateId = static_cast<uint32>(object.Spawn.TemplateId);
        if (!_sight.IsVisible(object.GlobalId) || !IsNpcTemplate(templateId))
            continue;
        bool const served = !sScriptMgr.GetNpcScripts(templateId).empty() || (quests && (QuestOfferProvider::Serves(*quests, templateId) || PersonaGoalProvider::Serves(*quests, templateId)));
        bool const offers = served && !MenuFor({ _worldGuid, object.GlobalId, templateId }, QuestProvidersFor(templateId)).Menu.GetEntries().empty();
        npcs.push_back({ object.GlobalId, { object.Spawn.Position.X, object.Spawn.Position.Y, object.Spawn.Position.Z }, radius, offers });
    }
    PlayerPosition const& at = _movement.GetPosition();
    NpcServiceRange::Changes const changes = _npcRange.Update({ at.X, at.Y, at.Z }, npcs);
    for (uint64 const left : changes.Left)
    {
        SendDmlMessage(GameMessages::LeaveServiceRange{ left });
        _questOffers.LeaveRange(left);
        auto const npc = std::ranges::find(npcs, left, &NpcServiceRange::Npc::GlobalId);
        if (npc == npcs.end())
        {
            LOG_INFO("server.gamesession", "Session {}'s wizard {} left the service range of NPC {}, which it no longer sees, at ({}, {}, {})", GetSessionId(), _worldGuid, left, at.X, at.Y, at.Z);
            continue;
        }
        if (!npc->Offers)
        {
            LOG_INFO("server.gamesession", "Session {}'s wizard {} left the service range of NPC {}, which no longer offers anything", GetSessionId(), _worldGuid, left);
            continue;
        }
        float const dx = npc->Position.X - at.X;
        float const dy = npc->Position.Y - at.Y;
        float const dz = npc->Position.Z - at.Z;
        LOG_INFO("server.gamesession", "Session {}'s wizard {} left the service range of NPC {} at ({}, {}, {}), {} units from it", GetSessionId(), _worldGuid, left, at.X, at.Y, at.Z,
            std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    for (uint64 const entered : changes.Entered)
        if (MapObject const* const npc = map->FindObject(entered))
            OfferNpcServices(*npc);
}

void GameSession::OfferNpcServices(MapObject const& npc, int32 reinteract)
{
    uint32 const templateId = static_cast<uint32>(npc.Spawn.TemplateId);
    NpcPresentation presentation;
    if (std::shared_ptr<ObjectTemplate const> const found = sObjectTemplateMgr.GetTemplate(templateId))
        if (std::optional<GameObjectTemplateView> const view = found->As<GameObjectTemplateView>())
        {
            presentation.DisplayKey = view->GetDisplayName();
            presentation.Icon = view->GetIcon();
        }
    NpcServiceTarget const target{ _worldGuid, npc.GlobalId, templateId };
    ServedMenu const served = MenuFor(target, QuestProvidersFor(target.TemplateId));
    NpcServiceMenu const& menu = served.Menu;
    QuestWireEncoder::BlobEncodeResult const encoded = NpcServiceMemento::Encode(sTypeRegistry.GetCatalog(), menu, presentation);
    if (!encoded.Ok())
    {
        LOG_ERROR("server.gamesession", "Session {} could not show wizard {} the services of NPC {} (template {}): {}", GetSessionId(), _worldGuid, npc.GlobalId, templateId, encoded.Error);
        return;
    }
    SendDmlMessage(GameMessages::SendNpcOptions{ npc.GlobalId, std::string(encoded.Bytes.begin(), encoded.Bytes.end()), reinteract });
    LOG_INFO("server.gamesession", "Session {}'s wizard {} entered the service range of NPC {} (template {}, mobile id {}, {} with icon {}) and was shown {} option(s)", GetSessionId(),
        _worldGuid, npc.GlobalId, templateId, npc.MobileId, Ambrose::ForLog(presentation.DisplayKey, 64), Ambrose::ForLog(presentation.Icon, 128), menu.GetEntries().size());
}

void GameSession::HandleInteractNpc(GameMessages::InteractNpc& message)
{
    if (!_mapId)
        return;
    if (message.ServiceName.empty())
    {
        LOG_INFO("server.gamesession", "HandleInteractNPC: session {}'s wizard {} closed the service of NPC {}", GetSessionId(), _worldGuid, message.GlobalId);
        return;
    }
    Map const* const map = sMapMgr.Find(*_mapId);
    MapObject const* const npc = map ? map->FindObject(message.GlobalId) : nullptr;
    if (!npc || !_npcRange.IsInside(message.GlobalId))
    {
        LOG_INFO("server.gamesession", "HandleInteractNPC: session {}'s wizard {} asked NPC {} for service index {} ({}) out of its range; ignored", GetSessionId(), _worldGuid,
            message.GlobalId, message.ServiceIndex, Ambrose::ForLog(message.ServiceName, 64));
        return;
    }
    NpcServiceTarget const target{ _worldGuid, npc->GlobalId, static_cast<uint32>(npc->Spawn.TemplateId) };
    ServedMenu const served = MenuFor(target, QuestProvidersFor(target.TemplateId));
    NpcServiceMenu const& menu = served.Menu;
    NpcServiceMenu::Entry const* const entry = menu.Find(message.ServiceIndex);
    if (!entry)
    {
        LOG_INFO("server.gamesession", "HandleInteractNPC: session {}'s wizard {} picked service index {} ({}) of NPC {}, whose menu holds {} option(s); ignored", GetSessionId(), _worldGuid,
            message.ServiceIndex, Ambrose::ForLog(message.ServiceName, 64), message.GlobalId, menu.GetEntries().size());
        return;
    }
    LOG_INFO("server.gamesession", "HandleInteractNPC: session {}'s wizard {} picked service index {} ({}) of NPC {} (template {}), reinteract {}, routed to {} option {}", GetSessionId(),
        _worldGuid, message.ServiceIndex, Ambrose::ForLog(message.ServiceName, 64), message.GlobalId, target.TemplateId, message.Reinteract, entry->Provider->GetName(), entry->Index);
    menu.Route(target, message.ServiceIndex);
}

void GameSession::HandleInteractObject(GameMessages::InteractObject& message)
{
    LOG_INFO("server.gamesession", "Session {}'s wizard {} interacted with object {} (template {}), which offers no options yet", GetSessionId(), _worldGuid, message.GlobalId,
        message.TemplateId);
}

void GameSession::HandleInteractOption(GameMessages::InteractOption& message)
{
    Map const* const map = _mapId ? sMapMgr.Find(*_mapId) : nullptr;
    MapObject const* const npc = map && _npcRange.IsInside(message.ObjectId) ? map->FindObject(message.ObjectId) : nullptr;
    if (npc)
    {
        NpcServiceTarget const target{ _worldGuid, npc->GlobalId, static_cast<uint32>(npc->Spawn.TemplateId) };
        ServedMenu const served = MenuFor(target, QuestProvidersFor(target.TemplateId));
        NpcServiceMenu const& menu = served.Menu;
        NpcServiceMenu::Entry const* const entry = message.OptionIndex >= 0 ? menu.Find(static_cast<uint32>(message.OptionIndex)) : nullptr;
        if (!entry)
        {
            LOG_INFO("server.gamesession", "HandleInteractOption: session {}'s wizard {} picked option {} of NPC {}, whose menu holds {} option(s); ignored", GetSessionId(), _worldGuid,
                message.OptionIndex, message.ObjectId, menu.GetEntries().size());
            return;
        }
        LOG_INFO("server.gamesession", "HandleInteractOption: session {}'s wizard {} picked option {} ({}) of NPC {} (template {}), routed to {} option {}", GetSessionId(), _worldGuid,
            message.OptionIndex, Ambrose::ForLog(entry->Option.ServiceName, 64), message.ObjectId, target.TemplateId, entry->Provider->GetName(), entry->Index);
        menu.Route(target, static_cast<uint32>(message.OptionIndex));
        return;
    }
    LOG_INFO("server.gamesession", "Session {}'s wizard {} picked option {} of object {}, which offers no options yet", GetSessionId(), _worldGuid, message.OptionIndex, message.ObjectId);
}

std::vector<std::unique_ptr<NpcServiceProvider>> GameSession::QuestProvidersFor(uint32 templateId)
{
    std::vector<std::unique_ptr<NpcServiceProvider>> providers;
    std::shared_ptr<QuestStore const> quests = sQuestMgr.GetQuests();
    if (!_questLog || !quests)
        return providers;
    if (PersonaGoalProvider::Serves(*quests, templateId))
        providers.push_back(std::make_unique<PersonaGoalProvider>(quests, *_questLog, [this](NpcServiceTarget const& target, PersonaGoals::Active const& goal) { CompletePersonaGoal(target, goal); }));
    if (QuestOfferProvider::Serves(*quests, templateId))
        providers.push_back(std::make_unique<QuestOfferProvider>(std::move(quests), *_questLog, std::make_shared<WizardRequirementContext const>(RequirementFacts(*_questLog)), [this](NpcServiceTarget const& target, QuestInfo const& quest) { OfferQuest(target, quest); },
            [this](NpcServiceTarget const& target, QuestInfo const& quest, QuestLogQuest const& held) { RemindOfQuest(target, quest, held); }));
    return providers;
}

void GameSession::OfferQuest(NpcServiceTarget const& target, QuestInfo const& quest)
{
    if (!_questLog)
        return;
    WizardRequirementContext const facts = RequirementFacts(*_questLog);
    QuestOffers::Encoded const encoded = QuestOffers::Encode(sTypeRegistry.GetCatalog(), quest, facts);
    if (!encoded.Ok())
    {
        LOG_ERROR("server.gamesession", "Session {} could not offer wizard {} quest {} from NPC {}: {}", GetSessionId(), _worldGuid, quest.Row.Name, target.Npc, encoded.Error);
        return;
    }
    GameMessages::ActorDialog dialog;
    dialog.MobileId = target.Npc;
    dialog.CompletionType = std::string(QuestOffers::CompletionType);
    dialog.Dialog = encoded.Dialog;
    dialog.RangeCheck = 1;
    SendDmlMessage(dialog);
    GameMessages::QuestOffer offer;
    offer.MobileId = target.Npc;
    offer.QuestName = quest.Row.Name;
    offer.QuestTitle = quest.Row.TitleKey;
    offer.QuestInfo = quest.Row.InfoKey;
    offer.Level = static_cast<int32>(quest.Row.Level);
    offer.Rewards = encoded.Rewards;
    offer.GoalData = encoded.GoalData;
    offer.Mainline = quest.Row.Mainline ? 1 : 0;
    SendDmlMessage(offer);
    _questOffers.Offer(quest.Row.Name, target.Npc);
    Dialogs::ActorDialog const prep = QuestOffers::BuildPrepDialog(quest, facts);
    _questDialogs.Play(target.Npc, QuestOffers::CompletionType, quest.Row.Name, prep);
    LOG_INFO("server.gamesession", "Session {} offered wizard {} quest {} ({}, level {}) from NPC {} (template {}) with its Prep dialog of {} line(s) and {} start goal(s)", GetSessionId(),
        _worldGuid, quest.Row.Name, Ambrose::ForLog(quest.Row.TitleKey, 64), quest.Row.Level, target.Npc, target.TemplateId, prep.Entries.size(),
        quest.StartGoals.size());
}

void GameSession::HandleInteractAvailableQuest(GameMessages::InteractAvailableQuest& message)
{
    Map const* const map = _mapId ? sMapMgr.Find(*_mapId) : nullptr;
    MapObject const* const npc = map && _npcRange.IsInside(message.MobileId) ? map->FindObject(message.MobileId) : nullptr;
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    if (!npc || !quests || !_questLog)
    {
        LOG_INFO("server.gamesession", "HandleInteractAvailableQuest: session {}'s wizard {} asked NPC {} for quest {} out of its range; ignored", GetSessionId(), _worldGuid, message.MobileId,
            Ambrose::ForLog(message.QuestName, 128));
        return;
    }
    uint32 const templateId = static_cast<uint32>(npc->Spawn.TemplateId);
    WizardRequirementContext const context = RequirementFacts(*_questLog);
    std::vector<QuestInfo const*> const available = QuestOffers::Available(*quests, templateId, *_questLog, context);
    auto const found = std::ranges::find_if(available, [&message](QuestInfo const* quest) { return quest->Row.Name == message.QuestName; });
    if (found == available.end())
    {
        LOG_INFO("server.gamesession", "HandleInteractAvailableQuest: session {}'s wizard {} asked NPC {} (template {}) for quest {}, which it does not offer the wizard; ignored", GetSessionId(),
            _worldGuid, message.MobileId, templateId, Ambrose::ForLog(message.QuestName, 128));
        return;
    }
    OfferQuest({ _worldGuid, npc->GlobalId, templateId }, **found);
}

void GameSession::HandleDeclineQuest(GameMessages::DeclineQuest& message)
{
    bool const declined = _questOffers.Decline(message.QuestName);
    LOG_INFO("server.gamesession", "Session {}'s wizard {} declined quest {}{}", GetSessionId(), _worldGuid, Ambrose::ForLog(message.QuestName, 128),
        declined ? "" : ", which no NPC was offering; nothing changes");
}

void GameSession::HandleAcceptQuest(GameMessages::AcceptQuest& message)
{
    QuestOfferVerdict const verdict = _questOffers.Check(message.QuestName, message.MobileId);
    if (verdict != QuestOfferVerdict::Standing)
    {
        LOG_WARN("server.gamesession", "Session {}'s wizard {} tried to accept quest {} from NPC {}, an offer {}; refused and nothing changes", GetSessionId(), _worldGuid,
            Ambrose::ForLog(message.QuestName, 128), message.MobileId, QuestOfferCache::GetVerdictName(verdict));
        return;
    }
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    QuestInfo const* const quest = quests ? quests->Find(message.QuestName) : nullptr;
    if (!quest || !StartQuest(*quest))
    {
        LOG_WARN("server.gamesession", "Session {}'s wizard {} accepted quest {} from NPC {}, which it already holds or which has no template; nothing changes", GetSessionId(), _worldGuid,
            Ambrose::ForLog(message.QuestName, 128), message.MobileId);
        return;
    }
    QuestLogQuest const* const held = _questLog->Find(quest->Row.Name);
    bool const started = held && PlayQuestDialog(message.MobileId, QuestDialogs::QuestStart, *quest, quest->Dialogs, QuestStore::TagStart, held->Gid, 0);
    LOG_INFO("server.gamesession", "Session {}'s wizard {} accepted quest {} from NPC {} with {} start goal(s){}", GetSessionId(), _worldGuid, Ambrose::ForLog(message.QuestName, 128),
        message.MobileId, quest->StartGoals.size(), started ? " and was shown its Start dialog" : "");
}

void GameSession::SendGoalDialog(NpcServiceTarget const& target, PersonaGoals::Active const& goal)
{
    PlayQuestDialog(target.Npc, PersonaGoals::CompletionType, *goal.Quest, goal.Goal->Dialogs, PersonaGoals::CompletionType, goal.Held->Gid, goal.HeldGoal->Gid);
}

bool GameSession::PlayQuestDialog(uint64 mobile, std::string_view completionType, QuestInfo const& quest, std::map<std::string, QuestDialog, std::less<>> const& dialogs,
    std::string_view tag, uint64 questGid, uint64 goalGid)
{
    if (!_questLog)
        return false;
    Dialogs::ActorDialog const lines = QuestBook::Dialog(dialogs, tag, RequirementFacts(*_questLog));
    if (lines.Entries.empty())
        return false;
    std::string problem;
    std::string const blob = QuestBook::Blob(QuestWireEncoder::BuildActorDialog(sTypeRegistry.GetCatalog(), lines), QuestWireEncoder::ActorDialogMask, "actor dialog", problem);
    if (!problem.empty())
    {
        LOG_ERROR("server.gamesession", "Session {} could not show wizard {} the {} dialog of {}: {}", GetSessionId(), _worldGuid, tag, quest.Row.Name, problem);
        return false;
    }
    GameMessages::ActorDialog dialog;
    dialog.MobileId = mobile;
    dialog.QuestId = questGid;
    dialog.GoalId = goalGid;
    dialog.CompletionType = std::string(completionType);
    dialog.Dialog = blob;
    dialog.RangeCheck = 1;
    SendDmlMessage(dialog);
    _questDialogs.Play(mobile, completionType, quest.Row.Name, lines);
    return true;
}

void GameSession::RemindOfQuest(NpcServiceTarget const& target, QuestInfo const& quest, QuestLogQuest const& held)
{
    bool const shown = PlayQuestDialog(target.Npc, QuestDialogs::Underway, quest, quest.Dialogs, QuestStore::TagUnderway, held.Gid, 0);
    LOG_INFO("server.gamesession", "Session {}'s wizard {} asked NPC {} (template {}) about quest {}, which it holds{}", GetSessionId(), _worldGuid, target.Npc, target.TemplateId,
        Ambrose::ForLog(quest.Row.Name, 128), shown ? ", and was shown its Underway dialog" : ", which has no Underway line for it");
}

void GameSession::CompletePersonaGoal(NpcServiceTarget const& target, PersonaGoals::Active const& goal)
{
    std::string const quest = goal.Held->Name;
    std::string const name = goal.HeldGoal->Name;
    uint64 const questGid = goal.Held->Gid;
    QuestInfo const& info = *goal.Quest;
    SendGoalDialog(target, goal);
    if (!_questLog || !_questLog->CompleteGoal(quest, name))
        return;
    PostQuestEvents();
    SaveQuestLogIfDirty();
    bool const questDone = !_questLog->HasQuest(quest);
    LOG_INFO("server.gamesession", "Session {}'s wizard {} talked to NPC {} (template {}) for goal {} of {}{}", GetSessionId(), _worldGuid, target.Npc, target.TemplateId,
        Ambrose::ForLog(name, 128), Ambrose::ForLog(quest, 128), questDone ? ", which completes the quest" : "");
    if (questDone)
        PlayQuestDialog(target.Npc, QuestDialogs::QuestComplete, info, info.Dialogs, QuestStore::TagComplete, questGid, 0);
    Map const* const map = questDone && _mapId ? sMapMgr.Find(*_mapId) : nullptr;
    if (MapObject const* const npc = map ? map->FindObject(target.Npc) : nullptr)
        OfferNpcServices(*npc, PersonaGoals::ReinteractAfterQuest);
}

void GameSession::HandleInteractCompleteGoal(GameMessages::InteractCompleteGoal& message)
{
    Map const* const map = _mapId ? sMapMgr.Find(*_mapId) : nullptr;
    MapObject const* const npc = map ? map->FindObject(message.MobileId) : nullptr;
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    PersonaGoals::Active goal;
    PersonaGoals::Verdict const verdict = npc && quests && _questLog ? PersonaGoals::Check(*quests, *_questLog, message.QuestId, message.GoalId, static_cast<uint32>(npc->Spawn.TemplateId),
        _npcRange.IsInside(message.MobileId), goal) : PersonaGoals::Verdict::WrongNpc;
    if (verdict != PersonaGoals::Verdict::Accepted)
    {
        LOG_INFO("server.gamesession", "HandleInteractCompleteGoal: session {}'s wizard {} asked NPC {} for the dialog of goal {} of quest {}, {}; ignored", GetSessionId(), _worldGuid,
            message.MobileId, message.GoalId, message.QuestId, PersonaGoals::GetVerdictName(verdict));
        return;
    }
    SendGoalDialog({ _worldGuid, npc->GlobalId, static_cast<uint32>(npc->Spawn.TemplateId) }, goal);
}

void GameSession::HandleCompletePersona(GameMessages::CompletePersona& message)
{
    Map const* const map = _mapId ? sMapMgr.Find(*_mapId) : nullptr;
    MapObject const* const npc = map ? map->FindObject(message.MobileId) : nullptr;
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    PersonaGoals::Active goal;
    PersonaGoals::Verdict const verdict = npc && quests && _questLog ? PersonaGoals::Check(*quests, *_questLog, message.QuestId, message.GoalId, static_cast<uint32>(npc->Spawn.TemplateId),
        _npcRange.IsInside(message.MobileId), goal) : PersonaGoals::Verdict::WrongNpc;
    if (verdict != PersonaGoals::Verdict::Accepted)
    {
        LOG_WARN("server.gamesession", "HandleCompletePersona: session {}'s wizard {} claimed to talk to NPC {} for goal {} of quest {}, {}; refused and nothing changes", GetSessionId(), _worldGuid,
            message.MobileId, message.GoalId, message.QuestId, PersonaGoals::GetVerdictName(verdict));
        return;
    }
    std::string const quest = goal.Held->Name;
    std::string const name = goal.HeldGoal->Name;
    if (!_questLog->CompleteGoal(quest, name))
        return;
    PostQuestEvents();
    SaveQuestLogIfDirty();
    LOG_INFO("server.gamesession", "Session {}'s wizard {} completed goal {} of {} by talking to NPC {}", GetSessionId(), _worldGuid, Ambrose::ForLog(name, 128), Ambrose::ForLog(quest, 128),
        message.MobileId);
}

void GameSession::HandleCompleteDialog(GameMessages::CompleteDialog& message)
{
    std::optional<std::string> const quest = _questDialogs.Complete(_worldGuid, message.MobileId, message.CompletionType, message.EntryEvent);
    if (!quest)
    {
        if (!message.EntryEvent.empty())
            LOG_INFO("server.gamesession", "HandleCompleteDialog: session {}'s wizard {} reported event {} of a {} dialog from NPC {}, which no dialog it is playing still carries; ignored", GetSessionId(),
                _worldGuid, Ambrose::ForLog(message.EntryEvent, 128), Ambrose::ForLog(message.CompletionType, 64), message.MobileId);
        return;
    }
    std::vector<std::string> const fired = _mapId ? PostZoneEvent(message.EntryEvent, std::chrono::steady_clock::now()) : std::vector<std::string>();
    LOG_INFO("server.gamesession", "Session {}'s wizard {} played dialog event {} of quest {} from NPC {}, which fired {}", GetSessionId(), _worldGuid, Ambrose::ForLog(message.EntryEvent, 128),
        Ambrose::ForLog(*quest, 128), message.MobileId, fired.empty() ? std::string("no trigger") : fmt::format("{}", fmt::join(fired, ", ")));
}

void GameSession::HandleInteractUnderwayQuest(GameMessages::InteractUnderwayQuest& message)
{
    Map const* const map = _mapId ? sMapMgr.Find(*_mapId) : nullptr;
    MapObject const* const npc = map && _npcRange.IsInside(message.MobileId) ? map->FindObject(message.MobileId) : nullptr;
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    QuestLogQuest const* held = nullptr;
    if (_questLog)
        if (auto const found = std::ranges::find(_questLog->GetQuests(), message.QuestId, &QuestLogQuest::Gid); found != _questLog->GetQuests().end())
            held = &*found;
    if (!npc || !quests || !held)
    {
        LOG_INFO("server.gamesession", "HandleInteractUnderwayQuest: session {}'s wizard {} asked NPC {} about quest {}, which it does not hold or out of the NPC's range; ignored", GetSessionId(),
            _worldGuid, message.MobileId, message.QuestId);
        return;
    }
    uint32 const templateId = static_cast<uint32>(npc->Spawn.TemplateId);
    std::vector<QuestInfo const*> const underway = QuestOffers::Underway(*quests, templateId, *_questLog);
    auto const found = std::ranges::find(underway, held->Name, [](QuestInfo const* quest) -> std::string const& { return quest->Row.Name; });
    if (found == underway.end())
    {
        LOG_INFO("server.gamesession", "HandleInteractUnderwayQuest: session {}'s wizard {} asked NPC {} (template {}) about quest {}, which it did not give; ignored", GetSessionId(), _worldGuid,
            message.MobileId, templateId, Ambrose::ForLog(held->Name, 128));
        return;
    }
    RemindOfQuest({ _worldGuid, npc->GlobalId, templateId }, **found, *held);
}

void GameSession::HandleRequestQuestDialog(GameMessages::RequestQuestDialog& message)
{
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    QuestInfo const* quest = nullptr;
    if (quests && _questLog)
        for (QuestLogQuest const& held : _questLog->GetQuests())
            if (QuestInfo const* const info = quests->Find(held.Name); info && info->Row.NameId == message.QuestNameId)
                quest = info;
    if (!quest)
    {
        LOG_INFO("server.gamesession", "HandleRequestQuestDialog: session {}'s wizard {} asked to review the dialog of quest name id {}, which it does not hold; ignored", GetSessionId(), _worldGuid,
            message.QuestNameId);
        return;
    }
    WizardRequirementContext const facts = RequirementFacts(*_questLog);
    Dialogs::ActorDialog lines = QuestBook::Dialog(quest->Dialogs, QuestStore::TagStart, facts);
    if (lines.Entries.empty())
        lines = QuestBook::Dialog(quest->Dialogs, QuestStore::TagPrep, facts);
    std::string problem;
    std::string const blob = QuestBook::Blob(QuestWireEncoder::BuildActorDialog(sTypeRegistry.GetCatalog(), lines), QuestWireEncoder::ActorDialogMask, "review dialog", problem);
    if (!problem.empty())
    {
        LOG_ERROR("server.gamesession", "Session {} could not show wizard {} the dialog of quest {} for review: {}", GetSessionId(), _worldGuid, quest->Row.Name, problem);
        return;
    }
    SendDmlMessage(GameMessages::QuestDialog{ message.QuestNameId, blob });
    LOG_INFO("server.gamesession", "Session {}'s wizard {} reviewed the {} dialog of quest {}, {} line(s)", GetSessionId(), _worldGuid, lines.Tag, Ambrose::ForLog(quest->Row.Name, 128),
        lines.Entries.size());
}
