/*
 * Project Ambrose by Imjustchico
 * Lists the messages the game server knows about: MSG_ATTACH handled the moment a client connects, because it is the only thing a client that has not attached yet may say, MSG_ATTACHFAILED refused inbound and declared as one the server sends, MSG_LOGINCOMPLETE declared as one the server sends and refused inbound, as are MSG_NEWOBJECT and MSG_REMOVEOBJECT, which bring an object into a wizard's view and take it away, and MSG_SERVERMOVE and MSG_MOVESTATE, which show other wizards moving, and MSG_ENTERSTATE, which puts another wizard's object in a state such as a jump, and MSG_ADDSPELLTOBOOK and MSG_REMOVESPELLFROMBOOK, which change a wizard's spellbook, MSG_CLIENTZONED from the WIZARD2 service handled once the wizard has been handed its object, the GAME moves, movement states and jumps a client sends from then on run on the world thread, where the wizard's place is kept, the WIZARD messages a client sends as it enters, taken from the moment it has its object because it sends them before it says it has loaded the zone, with the crown balance run on the world thread because the balance will be game state and the rest answered or logged where they arrive, the first in-world WizCombat handlers, the typed lines, quick chat phrases and emotes a wizard's client sends for the others around it, queued for the world thread, with the replies that show them declared as ones the server sends and refused from clients, and the SYSTEM and EXTENDEDBASE rules every app shares. Every other GAME, WIZARD, DOODLEDOUG_MESSAGES, WIZARD2 and WIZARD3 message is named once by PendingRest, because the world's services are the game server's own and hold hundreds of messages and the milestone that answers each will claim it by name then; until then one arrives as a message this server does not handle yet, which is reported, rather than as one it has never heard of. MSG_QUERY_LOGOUT and MSG_CLIENT_DISCONNECT are handled where they arrive, so a wizard that quits leaves at once, MSG_NOT_AFK runs on the world thread, where the AFK timer is kept, and MSG_QUERY_LOGOUT's reply, MSG_ZOMBIE_PLAYER, MSG_DISCONNECT_AFK and MSG_SERVERSHUTDOWN are declared as ones the server sends.
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
            Accept<&GameSession::HandleRequestRadialChat>(entered, MessageProcessing::Queued, "GameSession::HandleRequestRadialChat");
            Accept<&GameSession::HandleRequestRadialQuickChat>(entered, MessageProcessing::Queued, "GameSession::HandleRequestRadialQuickChat");
            Accept<&GameSession::HandleRequestRadialQuickChatExt>(entered, MessageProcessing::Queued, "GameSession::HandleRequestRadialQuickChatExt");
            Accept<&GameSession::HandleCoreEmote>(entered, MessageProcessing::Queued, "GameSession::HandleCoreEmote");
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
            Accept<&GameSession::HandleUsePotion>(entered, MessageProcessing::Queued, "GameSession::HandleUsePotion");

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
            Refuse(GameService, "MSG_RADIALCHAT");
            Refuse(GameService, "MSG_RADIALQUICKCHAT");
            Refuse(GameService, "MSG_RADIALQUICKCHATEXT");
            Refuse(WizardService, "MSG_ADDSPELLTOBOOK");
            Refuse(WizardService, "MSG_REMOVESPELLFROMBOOK");
            Refuse(WizardService, "MSG_UPDATEGOLD");
            Refuse(WizardService, "MSG_UPDATEHEALTH");
            Refuse(WizardService, "MSG_UPDATEMANA");
            Refuse(WizardService, "MSG_UPDATEPOTIONS");
            Refuse(WizardService, "MSG_UPDATEPOWERPIP");
            Refuse(WizardService, "MSG_UPDATESHADOWPIPRATING");

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
            Sends<RadialChat>();
            Sends<RadialQuickChat>();
            Sends<RadialQuickChatExt>();
            Sends<TimedAccessPasses>();
            Sends<SubscriberOnlyItems>();
            Sends<CombatPhaseForSpectators>();
            Sends<AddSpellToBook>();
            Sends<RemoveSpellFromBook>();
            Sends<UpdateGold>();
            Sends<UpdateHealth>();
            Sends<UpdateMana>();
            Sends<UpdatePotions>();
            Sends<UpdatePowerPip>();
            Sends<UpdateShadowPipRating>();
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
