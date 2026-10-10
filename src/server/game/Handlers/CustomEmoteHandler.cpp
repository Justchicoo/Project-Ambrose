/*
 * Project Ambrose by Imjustchico
 * Handles the radial menu's custom emote messages without changing ordinary chat emotes: a custom animation must be present in the user's templates and either the wizard's saved ownership masks or an emote item it holds, and a play request is relayed only with non-command valid UTF-16 text. The radial menus the client sends are kept only when every emote slot names an emote item the wizard holds, with that item's template; they are stored with its stats and sent back, which is what the client keeps them by, and a menu that fails is answered with the ones kept before.
 */

#include "AnimationListMgr.h"
#include "ChatMgr.h"
#include "CommandMgr.h"
#include "CustomEmoteMgr.h"
#include "EmoteRadialMenu.h"
#include "GameSession.h"
#include "Log.h"
#include "SpeechMessages.h"
#include "StringUtil.h"

#include <array>
#include <utility>

void GameSession::HandleCorePiiRadialMenuEmote(GameMessages::CorePiiRadialMenuEmote& message)
{
    if (!CanSpeak("a custom emote"))
        return;
    PlayerStats const* const stats = GetStats();
    std::shared_ptr<CustomEmoteStore const> const emotes = sCustomEmoteMgr.GetEmotes();
    if (stats == nullptr || !emotes->OwnsAnimation(message.EmoteAnimationName, stats->GetPurchasedCustomEmotes(), HeldEmoteTemplates()))
    {
        LOG_DEBUG("server.gamesession", "Session {} requested custom emote animation {} without owning it; nobody is shown it", GetSessionId(), Ambrose::ForLog(message.EmoteAnimationName, 64));
        return;
    }
    QueueEmote(message.EmoteAnimationName, message.ExcludeOriginator, "a custom emote");
}

void GameSession::HandleRequestPiiRadialMenuPlayEmote(GameMessages::RequestPiiRadialMenuPlayEmote& message)
{
    if (!CanSpeak("a custom emote"))
        return;
    PlayerStats const* const stats = GetStats();
    std::shared_ptr<CustomEmoteStore const> const emotes = sCustomEmoteMgr.GetEmotes();
    if (stats == nullptr || !emotes->OwnsAnimation(message.EmoteAnimationName, stats->GetPurchasedCustomEmotes(), HeldEmoteTemplates()))
    {
        LOG_DEBUG("server.gamesession", "Session {} requested custom emote animation {} without owning it; nobody is shown it", GetSessionId(), Ambrose::ForLog(message.EmoteAnimationName, 64));
        return;
    }
    if (!sAnimationListMgr.GetList()->Contains(message.EmoteAnimationName))
    {
        LOG_DEBUG("server.gamesession", "Session {} sent a custom emote naming the animation {}, which the install's animation list does not hold; nobody is shown it",
            GetSessionId(), Ambrose::ForLog(message.EmoteAnimationName, 64));
        return;
    }
    if (!ChatMgr::IsCustomEmoteText(message.EmoteText, sCommandMgr.GetPrefix()))
    {
        LOG_DEBUG("server.gamesession", "Session {} sent a custom emote with empty, unreadable or command text; nobody is shown it", GetSessionId());
        return;
    }
    Speech speech;
    speech.Kind = SpeechKind::CustomEmote;
    speech.WidePayload = std::move(message.EmoteText);
    speech.Animation = std::move(message.EmoteAnimationName);
    speech.SpeakerSees = true;
    QueueSpeech(std::move(speech), "a custom emote");
}

void GameSession::HearCustomEmote(ChatSpeaker const& speaker, Speech const& speech)
{
    SendDmlMessage(SpeechMessages::CustomEmote(speaker, speech));
}

std::vector<uint32> GameSession::HeldEmoteTemplates() const
{
    std::vector<uint32> held;
    if (_backpack)
        for (CharacterItem const& item : _backpack->GetItems())
            held.push_back(item.TemplateId);
    return held;
}

void GameSession::HandlePiiUpdateEmotesRadialMenu(GameMessages::PiiUpdateEmotesRadialMenu& message)
{
    if (!_player || !_backpack)
    {
        LOG_DEBUG("server.gamesession", "Session {} sent its radial menus before its wizard stands in the world; they are not kept", GetSessionId());
        return;
    }
    std::shared_ptr<CustomEmoteStore const> const emotes = sCustomEmoteMgr.GetEmotes();
    std::array<std::string, EmoteRadialMenu::MenuCount> menus{ std::move(message.RadialMenuData), std::move(message.RadialMenu2Data), std::move(message.RadialMenu3Data) };
    auto const heldTemplate = [this](uint64 itemGuid) -> std::optional<uint32>
    {
        CharacterItem const* const item = _backpack->Find(itemGuid);
        return item ? std::optional<uint32>(item->TemplateId) : std::nullopt;
    };
    auto const isEmote = [&emotes](uint32 templateId) { return emotes->IsEmoteTemplate(templateId); };
    std::size_t emoteSlots = 0;
    std::size_t phraseSlots = 0;
    for (std::size_t menu = 0; menu < menus.size(); ++menu)
    {
        std::string problem;
        std::optional<std::vector<RadialMenuSlot>> const slots = EmoteRadialMenu::Parse(menus[menu], problem);
        if (!slots || !EmoteRadialMenu::CheckSlots(*slots, heldTemplate, isEmote, problem))
        {
            LOG_INFO("server.gamesession", "Session {} kept wizard {}'s radial menus as they were, since radial menu {} it sent {}", GetSessionId(), _worldGuid, menu + 1, problem);
            SendRadialMenus();
            return;
        }
        for (RadialMenuSlot const& slot : *slots)
            ++(slot.EmoteId != 0 ? emoteSlots : phraseSlots);
    }
    if (_player->SetRadialMenus(std::move(menus)))
        SaveStatsIfDirty();
    LOG_INFO("server.gamesession", "Session {} set wizard {}'s radial menus to {} emote slot(s) and {} chat phrase slot(s), which it keeps and sends back", GetSessionId(), _worldGuid,
        emoteSlots, phraseSlots);
    SendRadialMenus();
}

void GameSession::SendRadialMenus()
{
    PlayerStats const* const stats = GetStats();
    if (stats == nullptr)
        return;
    std::array<std::string, 3> const& menus = stats->GetRadialMenus();
    GameMessages::PiiUpdateEmotesRadialMenu update;
    update.RadialMenuData = menus[0];
    update.RadialMenu2Data = menus[1];
    update.RadialMenu3Data = menus[2];
    SendDmlMessage(update);
}
