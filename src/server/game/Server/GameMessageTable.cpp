/*
 * Project Ambrose by Imjustchico
 * The game message table's accepted and sent messages: attach and entry handling, logout and disconnect, AFK and movement on the world thread, link-dead notices, shutdown messages and existing shop, spellbook and combat rules, with each remaining client message covered by a pending-service rule until its owning milestone handles it.
 */

#include "GameMessageTable.h"
#include "SystemMessageRules.h"

namespace
{
    using namespace GameMessages;
    using SystemMessages::ExtendedBaseService;
    using SystemMessages::SystemService;

    class GameRules : public MessageHandlerTable<GameSession>
    {
    public:
        GameRules() : MessageHandlerTable<GameSession>("gameserver", { SystemService, ExtendedBaseService, GameService, WizardService, CombatService, Wizard2Service, Wizard3Service },
            QueuedMessageDrain::DrainedByOwner)
        {
            Accept<&GameSession::HandleAttach>(SessionStatuses::Connected, MessageProcessing::InPlace, "GameSession::HandleAttach");
            Accept<&GameSession::HandleClientZoned>(SessionStatuses::LoggedIn | SessionStatuses::InWorld, MessageProcessing::Queued, "GameSession::HandleClientZoned");

            SessionStatusMask const entered = SessionStatuses::LoggedIn | SessionStatuses::InWorld;
            Accept<&GameSession::HandleClientMove>(entered, MessageProcessing::Queued, "GameSession::HandleClientMove");
            Accept<&GameSession::HandleClientMoveState>(entered, MessageProcessing::Queued, "GameSession::HandleClientMoveState");
            Accept<&GameSession::HandleJump>(entered, MessageProcessing::Queued, "GameSession::HandleJump");
            Accept<&GameSession::HandleQueryLogout>(entered, MessageProcessing::InPlace, "GameSession::HandleQueryLogout");
            Accept<&GameSession::HandleClientDisconnect>(entered, MessageProcessing::InPlace, "GameSession::HandleClientDisconnect");
            Accept<&GameSession::HandleNotAfk>(entered, MessageProcessing::Queued, "GameSession::HandleNotAfk");
            Accept<&GameSession::HandleGetTimedAccessPasses>(entered, MessageProcessing::InPlace, "GameSession::HandleGetTimedAccessPasses");
            Accept<&GameSession::HandleGetSubscriberOnlyItems>(entered, MessageProcessing::InPlace, "GameSession::HandleGetSubscriberOnlyItems");
            Accept<&GameSession::HandleCrownBalance>(entered, MessageProcessing::Queued, "GameSession::HandleCrownBalance");
            Accept<&GameSession::HandleDoneShopping>(entered, MessageProcessing::InPlace, "GameSession::HandleDoneShopping");
            Accept<&GameSession::HandleLogClientResolution>(entered, MessageProcessing::InPlace, "GameSession::HandleLogClientResolution");
            Accept<&GameSession::HandleLogPatchClientPatchTime>(entered, MessageProcessing::InPlace, "GameSession::HandleLogPatchClientPatchTime");
            Accept<&GameSession::HandleQuestFinderOption>(entered, MessageProcessing::InPlace, "GameSession::HandleQuestFinderOption");

            SessionStatusMask const inWorld = SessionStatuses::InWorld;
            Accept<&GameSession::HandleCombatMove>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatMove");
            Accept<&GameSession::HandleCombatDraw>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatDraw");
            Accept<&GameSession::HandleCombatAFK>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatAFK");
            Accept<&GameSession::HandleCombatVictory>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatVictory");
            Accept<&GameSession::HandlePetWillCast>(inWorld, MessageProcessing::InPlace, "GameSession::HandlePetWillCast");
            Accept<&GameSession::HandleDismissSummon>(inWorld, MessageProcessing::InPlace, "GameSession::HandleDismissSummon");
            Accept<&GameSession::HandleCombatCheat>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatCheat");

            Refuse(GameService, "MSG_ATTACHFAILED");
            Refuse(GameService, "MSG_LOGINCOMPLETE");
            Refuse(GameService, "MSG_SERVERMOVE");
            Refuse(GameService, "MSG_MOVESTATE");
            Refuse(GameService, "MSG_ENTERSTATE");
            Refuse(WizardService, "MSG_ADDSPELLTOBOOK");
            Refuse(WizardService, "MSG_REMOVESPELLFROMBOOK");

            SessionStatusMask const any = SessionStatuses::Connected | SessionStatuses::Authenticated | SessionStatuses::CharacterSelected | SessionStatuses::LoggedIn | SessionStatuses::InWorld;
            PendingRest(GameService, any);
            PendingRest(CombatService, any);
            PendingRest(WizardService, any);
            PendingRest(Wizard2Service, any);
            PendingRest(Wizard3Service, any);

            Sends<AttachFailed>();
            Sends<LoginComplete>();
            Sends<NewObject>();
            Sends<RemoveObject>();
            Sends<ServerMove>();
            Sends<MoveState>();
            Sends<EnterState>();
            Sends<TimedAccessPasses>();
            Sends<SubscriberOnlyItems>();
            Sends<CombatPhaseForSpectators>();
            Sends<AddSpellToBook>();
            Sends<RemoveSpellFromBook>();
            Sends<QueryLogout>();
            Sends<ZombiePlayer>();
            Sends<DisconnectAfk>();
            Sends<ServerShutdown>();

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<GameSession> const& GameMessageTable::Get()
{
    static GameRules const table;
    return table;
}
