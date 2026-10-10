/*
 * Project Ambrose by Imjustchico
 * The NPC service menu: after each move the NPCs a wizard sees, objects whose template carries NPCBehavior, are checked against their service range, only an NPC some provider gives an option counting, as only a quest, a shop or a script's option earns a prompt, and a wizard entering one is sent MSG_SENDNPCOPTIONS with the NPC's global id, as the client finds an NPC by it, and the ServiceMementoBase of its menu, the options every NpcScript serving its template offers, and one leaving is sent MSG_LEAVESERVICERANGE, once per crossing. MSG_INTERACTNPC, a click on a menu, is routed by its flat service index to the provider that owns that option, rebuilt as it was offered with its providers, which the menu's entries point at and so live as long as it does; an empty service name closes a shop, and an NPC the wizard is not in range of, or an index past its menu, is logged and ignored. MSG_INTERACTOPTION on an NPC the wizard is in range of is how the client answers its prompt, its options being InteractableOptions, and is routed the same way by option index; on any other object it, and MSG_INTERACTOBJECT, are logged until a provider offers interactables.
 */

#include "GameSession.h"
#include "Log.h"
#include "MapMgr.h"
#include "NpcServiceMemento.h"
#include "ObjectTemplateMgr.h"
#include "ObjectViews.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <cmath>
#include <memory>

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

    ServedMenu MenuFor(NpcServiceTarget const& target)
    {
        ServedMenu served;
        served.Providers = ProvidersFor(target.TemplateId);
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
    std::vector<NpcServiceRange::Npc> npcs;
    for (MapObject const& object : map->GetObjects())
    {
        uint32 const templateId = static_cast<uint32>(object.Spawn.TemplateId);
        if (!_sight.IsVisible(object.GlobalId) || !IsNpcTemplate(templateId))
            continue;
        bool const offers = !sScriptMgr.GetNpcScripts(templateId).empty() && !MenuFor({ _worldGuid, object.GlobalId, templateId }).Menu.GetEntries().empty();
        npcs.push_back({ object.GlobalId, { object.Spawn.Position.X, object.Spawn.Position.Y, object.Spawn.Position.Z }, radius, offers });
    }
    PlayerPosition const& at = _movement.GetPosition();
    NpcServiceRange::Changes const changes = _npcRange.Update({ at.X, at.Y, at.Z }, npcs);
    for (uint64 const left : changes.Left)
    {
        SendDmlMessage(GameMessages::LeaveServiceRange{ left });
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

void GameSession::OfferNpcServices(MapObject const& npc)
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
    ServedMenu const served = MenuFor(target);
    NpcServiceMenu const& menu = served.Menu;
    QuestWireEncoder::BlobEncodeResult const encoded = NpcServiceMemento::Encode(sTypeRegistry.GetCatalog(), menu, presentation);
    if (!encoded.Ok())
    {
        LOG_ERROR("server.gamesession", "Session {} could not show wizard {} the services of NPC {} (template {}): {}", GetSessionId(), _worldGuid, npc.GlobalId, templateId, encoded.Error);
        return;
    }
    SendDmlMessage(GameMessages::SendNpcOptions{ npc.GlobalId, std::string(encoded.Bytes.begin(), encoded.Bytes.end()), 0 });
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
    ServedMenu const served = MenuFor(target);
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
        ServedMenu const served = MenuFor(target);
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
