/*
 * Project Ambrose by Imjustchico
 * Answers the WIZARD messages a client sends as it enters the world: its timed access passes and subscriber-only items with empty lists of the classes the client's own handlers load, ActiveTimedAccessPassList and SubscriberOnlyItemsList, written raw because the client reads them with no envelope and no flags word, and its crown balance with none, the only fields the client reads back being Failure and TotalCrowns, until accounts keep crowns; and logs the notes it sends about its screen, its patch time, the end of its shopping and its quest finder.
 */

#include "GameSession.h"
#include "Log.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <optional>
#include <string>
#include <string_view>

namespace
{
    constexpr char const* WizardLog = "server.gamesession";

    std::optional<std::string> EncodeEmptyList(uint16 sessionId, std::string_view message, std::string_view className)
    {
        TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
        PropertyObjectPtr const list = catalog ? PropertyObject::Create(catalog, className) : nullptr;
        ObjectField const* const field = ObjectFields::Find(message, "Data");
        if (!list || !field)
        {
            LOG_ERROR(WizardLog, "Session {} gets no {}, because {}", sessionId, message, !list ? fmt::format("the type dump has no {}", className) : std::string("no field describes its Data"));
            return std::nullopt;
        }
        EncodeResult const encoded = ObjectSerializer::EncodeField(*field, list.get());
        if (!encoded.Ok())
        {
            LOG_ERROR(WizardLog, "Session {} gets no {}, because its {} does not encode: {}", sessionId, message, className, encoded.Detail);
            return std::nullopt;
        }
        return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
    }
}

void GameSession::HandleGetTimedAccessPasses(GameMessages::GetTimedAccessPasses&)
{
    std::optional<std::string> data = EncodeEmptyList(GetSessionId(), GameMessages::TimedAccessPasses::Tag, "class ActiveTimedAccessPassList");
    if (!data)
        return;
    GameMessages::TimedAccessPasses reply;
    reply.Data = std::move(*data);
    SendDmlMessage(reply);
    LOG_DEBUG(WizardLog, "Session {} asked for its timed access passes and was told it has none", GetSessionId());
}

void GameSession::HandleGetSubscriberOnlyItems(GameMessages::GetSubscriberOnlyItems&)
{
    std::optional<std::string> data = EncodeEmptyList(GetSessionId(), GameMessages::SubscriberOnlyItems::Tag, "class SubscriberOnlyItemsList");
    if (!data)
        return;
    GameMessages::SubscriberOnlyItems reply;
    reply.Data = std::move(*data);
    SendDmlMessage(reply);
    LOG_DEBUG(WizardLog, "Session {} asked which items only subscribers may use and was told none", GetSessionId());
}

void GameSession::HandleCrownBalance(GameMessages::CrownBalance&)
{
    GameMessages::CrownBalance reply;
    reply.CharacterId = GetCharacterId();
    SendDmlMessage(reply);
    LOG_DEBUG(WizardLog, "Session {} asked for its crown balance and was told 0, because accounts keep no crowns yet", GetSessionId());
}

void GameSession::HandleDoneShopping(GameMessages::DoneShopping& message)
{
    LOG_DEBUG(WizardLog, "Session {} is done shopping, transaction {}", GetSessionId(), message.TransactionId);
}

void GameSession::HandleLogClientResolution(GameMessages::LogClientResolution& message)
{
    LOG_DEBUG(WizardLog, "Session {} runs its client at {}x{}, {}{}", GetSessionId(), message.ScreenWidth, message.ScreenHeight,
        message.FullScreen ? "full screen" : "in a window", message.ClassicMode ? ", in classic mode" : "");
}

void GameSession::HandleLogPatchClientPatchTime(GameMessages::LogPatchClientPatchTime& message)
{
    LOG_DEBUG(WizardLog, "Session {} reports a patch client patch time of {}", GetSessionId(), message.PatchClientPatchTime);
}

void GameSession::HandleQuestFinderOption(GameMessages::QuestFinderOption& message)
{
    LOG_DEBUG(WizardLog, "Session {} turned its quest finder {}", GetSessionId(), message.Enable ? "on" : "off");
}
