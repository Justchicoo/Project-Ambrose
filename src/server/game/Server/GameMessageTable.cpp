/*
 * Project Ambrose by Imjustchico
 * Lists the messages the game server knows about: MSG_ATTACH handled the moment a client connects, because it is the only thing a client that has not attached yet may say, MSG_ATTACHFAILED refused inbound and declared as one the server sends, MSG_LOGINCOMPLETE declared as one the server sends and refused inbound, MSG_CLIENTZONED from the WIZARD2 service handled once the wizard has been handed its object, and the SYSTEM and EXTENDEDBASE rules every app shares. Every other GAME, WIZARD, WIZARD2 and WIZARD3 message is named once by PendingRest, because the world's services are the game server's own and hold hundreds of messages and the milestone that answers each will claim it by name then; until then one arrives as a message this server does not handle yet, which is reported, rather than as one it has never heard of.
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
        GameRules() : MessageHandlerTable<GameSession>("gameserver", { SystemService, ExtendedBaseService, GameService, WizardService, Wizard2Service, Wizard3Service },
            QueuedMessageDrain::DrainedByOwner)
        {
            Accept<&GameSession::HandleAttach>(SessionStatuses::Connected, MessageProcessing::InPlace, "GameSession::HandleAttach");
            Accept<&GameSession::HandleClientZoned>(SessionStatuses::LoggedIn | SessionStatuses::InWorld, MessageProcessing::Queued, "GameSession::HandleClientZoned");

            Refuse(GameService, "MSG_ATTACHFAILED");
            Refuse(GameService, "MSG_LOGINCOMPLETE");

            SessionStatusMask const any = SessionStatuses::Connected | SessionStatuses::Authenticated | SessionStatuses::CharacterSelected | SessionStatuses::LoggedIn | SessionStatuses::InWorld;
            PendingRest(GameService, any);
            PendingRest(WizardService, any);
            PendingRest(Wizard2Service, any);
            PendingRest(Wizard3Service, any);

            Sends<AttachFailed>();
            Sends<LoginComplete>();

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<GameSession> const& GameMessageTable::Get()
{
    static GameRules const table;
    return table;
}
