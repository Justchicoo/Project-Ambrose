/*
 * Project Ambrose by Imjustchico
 * Lists the messages the game server knows about: MSG_ATTACH handled the moment a client connects, because it is the only thing a client that has not attached yet may say, MSG_ATTACHFAILED refused inbound and declared as one the server sends, MSG_LOGINCOMPLETE declared as one the server sends and refused inbound, MSG_CLIENTZONED from the WIZARD2 service handled once the wizard has been handed its object, the GAME moves, movement states and jumps a client sends from then on run on the world thread, where the wizard's place is kept, the WIZARD messages a client sends as it enters, taken from the moment it has its object because it sends them before it says it has loaded the zone, with the crown balance run on the world thread because the balance will be game state and the rest answered or logged where they arrive, the first in-world WizCombat handlers, and the SYSTEM and EXTENDEDBASE rules every app shares. Every other GAME, WIZARD, DOODLEDOUG_MESSAGES, WIZARD2 and WIZARD3 message is named once by PendingRest, because the world's services are the game server's own and hold hundreds of messages and the milestone that answers each will claim it by name then; until then one arrives as a message this server does not handle yet, which is reported, rather than as one it has never heard of.
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

            SessionStatusMask const any = SessionStatuses::Connected | SessionStatuses::Authenticated | SessionStatuses::CharacterSelected | SessionStatuses::LoggedIn | SessionStatuses::InWorld;
            PendingRest(GameService, any);
            PendingRest(CombatService, any);
            PendingRest(WizardService, any);
            PendingRest(Wizard2Service, any);
            PendingRest(Wizard3Service, any);

            Sends<AttachFailed>();
            Sends<LoginComplete>();
            Sends<TimedAccessPasses>();
            Sends<SubscriberOnlyItems>();
            Sends<CombatPhaseForSpectators>();

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<GameSession> const& GameMessageTable::Get()
{
    static GameRules const table;
    return table;
}
