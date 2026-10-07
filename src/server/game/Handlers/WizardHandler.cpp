/*
 * Project Ambrose by Imjustchico
 * Answers the WIZARD messages a client sends as it enters the world: its timed access passes and subscriber-only items with empty lists of the classes the client's own handlers load, ActiveTimedAccessPassList and SubscriberOnlyItemsList, written raw because the client reads them with no envelope and no flags word, and its crown balance with none, the only fields the client reads back being Failure and TotalCrowns, until accounts keep crowns; tells a wizard entering the world which badges it holds with MSG_BADGES, whose BadgeInfo and BadgeFilterInfo are enveloped BadgeInfoList and BadgeFilterInfoList objects, which the client cannot read unwrapped and reads only when Add is set, since GameClient::MSG_Badges adds a BadgeInfoList only then and shows no earned popup unless Display is set: its school's badge, the badge of the install's Badges table whose text is the school's title, such as Pyromancer, sent under that badge's key, shown complete in the Miscellaneous filter of the install's BadgeFilterDescriptions.xml with the install's first badge art, a badge and a filter each known by the client's string hash of its name; and logs the notes it sends about its screen, the end of its shopping and its quest finder; and relays a player's spellbook wizbang to the wizards in the same zone instance.
 */

#include "GameSession.h"
#include "LocaleStore.h"
#include "Log.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PlayerLevelMgr.h"
#include "PlayerWizBang.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr char const* WizardLog = "server.gamesession";

    std::optional<std::string> EncodeEmptyList(uint16 sessionId, std::string_view message, std::string_view className, std::string_view fieldName = "Data")
    {
        TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
        PropertyObjectPtr const list = catalog ? PropertyObject::Create(catalog, className) : nullptr;
        ObjectField const* const field = ObjectFields::Find(message, fieldName);
        if (!list || !field)
        {
            LOG_ERROR(WizardLog, "Session {} gets no {}, because {}", sessionId, message, !list ? fmt::format("the type dump has no {}", className) : fmt::format("no field describes its {}", fieldName));
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

void GameSession::SendBadges()
{
    constexpr std::string_view BadgeStem = "Badges_";
    constexpr std::string_view RankFilter = "Miscellaneous";
    constexpr std::string_view RankArt = "GUI/Art/Art_Badge01.dds";

    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    ObjectField const* const infoField = ObjectFields::Find(GameMessages::Badges::Tag, "BadgeInfo");
    ObjectField const* const filterField = ObjectFields::Find(GameMessages::Badges::Tag, "BadgeFilterInfo");
    PropertyObjectPtr list = catalog ? PropertyObject::Create(catalog, "class BadgeInfoList") : nullptr;
    PropertyObjectPtr filters = catalog ? PropertyObject::Create(catalog, "class BadgeFilterInfoList") : nullptr;
    if (!infoField || !filterField || !list || !filters)
    {
        LOG_ERROR(WizardLog, "Session {} gets no badges, because the type dump or the message fields do not describe them", GetSessionId());
        return;
    }

    std::vector<std::string> ranks;
    std::shared_ptr<PlayerLevelSet const> const levels = sPlayerLevelMgr.GetLevels();
    std::shared_ptr<LocaleTable const> const texts = sLocaleStore.IsLoaded() ? sLocaleStore.GetTable() : nullptr;
    MagicSchool const* const school = levels && _stats ? levels->FindSchool(_stats->GetSchoolId()) : nullptr;
    std::string const* const title = school && texts ? texts->Find(fmt::format("MagicSchools_{}Title", school->Name)) : nullptr;
    if (title)
        ranks.push_back(*title);

    PropertyValue::List badges;
    uint32 current = 0;
    for (std::string const& rank : ranks)
    {
        std::string key;
        for (std::string const& found : texts->FindKeys(rank))
            if (found.starts_with(BadgeStem))
            {
                key = found;
                break;
            }
        if (key.empty())
            continue;
        PropertyObjectPtr badge = PropertyObject::Create(catalog, "class BadgeInfo");
        if (!badge)
            break;
        current = StringHash::KiStringHash(key);
        badge->Set("m_badgeNameID", current);
        badge->Set("m_badgeTitle", key);
        badge->Set("m_badgeInfo", key);
        badge->Set("m_badgeProgressInfo", key);
        badge->Set("m_badgeProgress", int32{ 1 });
        badge->Set("m_badgeMax", int32{ 1 });
        badge->Set("m_originalProgress", int32{ 1 });
        badge->Set("m_badgeComplete", true);
        badge->Set("m_index", static_cast<uint32>(badges.size()));
        badge->Set("m_badgeImage", std::string(RankArt));
        badge->Set("m_badgeFilterID", StringHash::KiStringHash(RankFilter));
        badges.emplace_back(std::move(badge));
    }
    uint32 const total = static_cast<uint32>(badges.size());
    list->Set("m_badges", std::move(badges));
    if (total > 0)
    {
        PropertyValue::List filterList;
        if (PropertyObjectPtr filter = PropertyObject::Create(catalog, "class BadgeFilterInfo"))
        {
            filter->Set("m_badgeFilterNameID", StringHash::KiStringHash(RankFilter));
            filter->Set("m_numberOfBadges", static_cast<int32>(total));
            filterList.emplace_back(std::move(filter));
        }
        filters->Set("m_badgeFilterInfoList", std::move(filterList));
    }

    EncodeResult const info = ObjectSerializer::EncodeField(*infoField, list.get());
    EncodeResult const filterInfo = ObjectSerializer::EncodeField(*filterField, filters.get());
    if (!info.Ok() || !filterInfo.Ok())
    {
        LOG_ERROR(WizardLog, "Session {} gets no badges, because they do not encode: {}", GetSessionId(), info.Ok() ? filterInfo.Detail : info.Detail);
        return;
    }
    GameMessages::Badges message;
    message.UpdateAll = 1;
    message.CurrentBadge = current;
    message.TotalBadges = total;
    message.Add = 1;
    message.BadgeInfo.assign(info.Bytes.begin(), info.Bytes.end());
    message.BadgeFilterInfo.assign(filterInfo.Bytes.begin(), filterInfo.Bytes.end());
    message.LastSegment = 1;
    SendDmlMessage(message);
    LOG_DEBUG(WizardLog, "Session {} told its wizard it holds {} badge(s): {}", GetSessionId(), total, fmt::join(ranks, ", "));
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

void GameSession::HandleQuestFinderOption(GameMessages::QuestFinderOption& message)
{
    LOG_DEBUG(WizardLog, "Session {} turned its quest finder {}", GetSessionId(), message.Enable ? "on" : "off");
}

void GameSession::HandlePlayerWizBang(GameMessages::PlayerWizBang& message)
{
    std::optional<uint32> const mapId = GetMapId();
    if (!mapId || !IsShown())
    {
        LOG_WARN(WizardLog, "Session {} sent MSG_PLAYERWIZBANG while wizard {} has no shown world object", GetSessionId(), GetWorldGuid());
        return;
    }

    uint32 const wizBangId = PlayerWizBang::IdForState(message.StateName);
    if (_wizBangId == wizBangId)
        return;
    _wizBangId = wizBangId;
    _pendingWizBang = wizBangId;
}
