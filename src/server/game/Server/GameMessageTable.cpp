/*
 * Project Ambrose by Imjustchico
 * Lists the messages the game server knows about: MSG_ATTACH is handled on connection, MSG_USEPOTION is queued for the world thread, WIZARD HUD updates and ELIXIRSTATECHANGE are declared as server messages and refused inbound, as are MSG_LOGINCOMPLETE and the object and movement messages; client movement, chat, spellbook, combat and the other entered-world messages are handled at their proper session states, and every message the server sends is declared by its typed fields. Every other GAME, WIZARD, DOODLEDOUG_MESSAGES, WIZARD2 and WIZARD3 message is named once by PendingRest, because the world's services are the game server's own and hold hundreds of messages and the milestone that answers each will claim it by name then; until then one arrives as a message this server does not handle yet, which is reported, rather than as one it has never heard of.
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

            SessionStatusMask const inWorld = SessionStatuses::InWorld;
            Accept<&GameSession::HandleUsePotion>(inWorld, MessageProcessing::Queued, "GameSession::HandleUsePotion");
            Accept<&GameSession::HandlePlayerWizBang>(inWorld, MessageProcessing::Queued, "GameSession::HandlePlayerWizBang");
            Accept<&GameSession::HandleCombatMove>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatMove");
            Accept<&GameSession::HandleCombatDraw>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatDraw");
            Accept<&GameSession::HandleCombatAFK>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatAFK");
            Accept<&GameSession::HandleCombatVictory>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatVictory");
            Accept<&GameSession::HandlePetWillCast>(inWorld, MessageProcessing::InPlace, "GameSession::HandlePetWillCast");
            Accept<&GameSession::HandleDismissSummon>(inWorld, MessageProcessing::InPlace, "GameSession::HandleDismissSummon");
            Accept<&GameSession::HandleCombatCheat>(inWorld, MessageProcessing::InPlace, "GameSession::HandleCombatCheat");

            Refuse(GameService, "MSG_ATTACHFAILED");
            Refuse(GameService, "MSG_BADGES");
            Refuse(GameService, "MSG_LOGINCOMPLETE");
            Refuse(GameService, "MSG_SERVERMOVE");
            Refuse(GameService, "MSG_MOVESTATE");
            Refuse(GameService, "MSG_ENTERSTATE");
            Refuse(GameService, "MSG_WIZBANG");
            Refuse(GameService, "MSG_RADIALCHAT");
            Refuse(GameService, "MSG_RADIALQUICKCHAT");
            Refuse(GameService, "MSG_RADIALQUICKCHATEXT");
            Refuse(WizardService, "MSG_ADDSPELLTOBOOK");
            Refuse(WizardService, "MSG_REMOVESPELLFROMBOOK");
            Refuse(WizardService, "MSG_UPDATEHEALTH");
            Refuse(WizardService, "MSG_UPDATEMANA");
            Refuse(WizardService, "MSG_UPDATEGOLD");
            Refuse(WizardService, "MSG_UPDATEPOWERPIP");
            Refuse(WizardService, "MSG_UPDATEPOTIONS");
            Refuse(WizardService, "MSG_UPDATESHADOWPIPRATING");
            Refuse(WizardService, "MSG_ELIXIRSTATECHANGE");
            Refuse(Wizard2Service, "MSG_UPDATEMAXSHADOWPIPS");
            Refuse(Wizard2Service, "MSG_UPDATEPIPCONVERSION");
            Refuse(Wizard3Service, "MSG_UPDATEARCHMASTERY");

            SessionStatusMask const any = SessionStatuses::Connected | SessionStatuses::Authenticated | SessionStatuses::CharacterSelected | SessionStatuses::LoggedIn | SessionStatuses::InWorld;
            PendingRest(GameService, any);
            PendingRest(CombatService, any);
            PendingRest(WizardService, any);
            PendingRest(Wizard2Service, any);
            PendingRest(Wizard3Service, any);

            Sends<AttachFailed>();
            Sends<Badges>();
            Sends<LoginComplete>();
            Sends<NewObject>();
            Sends<RemoveObject>();
            Sends<ServerMove>();
            Sends<MoveState>();
            Sends<EnterState>();
            Sends<WizBang>();
            Sends<RadialChat>();
            Sends<RadialQuickChat>();
            Sends<RadialQuickChatExt>();
            Sends<TimedAccessPasses>();
            Sends<SubscriberOnlyItems>();
            Sends<CombatPhaseForSpectators>();
            Sends<AddSpellToBook>();
            Sends<RemoveSpellFromBook>();
            Sends<QueryLogout>();
            Sends<ClientDisconnect>();
            Sends<ZombiePlayer>();
            Sends<DisconnectAfk>();
            Sends<ServerShutdown>();
            Sends<UpdateHealth>();
            Sends<UpdateMana>();
            Sends<UpdateGold>();
            Sends<UpdatePowerPip>();
            Sends<UpdatePotions>();
            Sends<UpdateShadowPipRating>();
            Sends<ElixirStateChange>();
            Sends<UpdateMaxShadowPips>();
            Sends<UpdatePipConversion>();
            Sends<UpdateArchmastery>();

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<GameSession> const& GameMessageTable::Get()
{
    static GameRules const table;
    return table;
}
