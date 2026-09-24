/*
 * Project Ambrose by Imjustchico
 * Lists the GAME messages the game server knows about: MSG_ATTACH handled the moment a client connects, because it is the only thing a client that has not attached yet may say, MSG_ATTACHFAILED refused inbound and declared as one the server sends, and the SYSTEM and EXTENDEDBASE rules every app shares. Every other GAME message is named once by PendingRest, because the service holds about two hundred and fifty of them and the milestone that answers each will claim it by name then; until then one arrives as a message this server does not handle yet, which is reported, rather than as one it has never heard of.
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
        GameRules() : MessageHandlerTable<GameSession>("gameserver", { SystemService, ExtendedBaseService, GameService })
        {
            Accept<&GameSession::HandleAttach>(SessionStatuses::Connected, MessageProcessing::InPlace, "GameSession::HandleAttach");

            Refuse(GameService, "MSG_ATTACHFAILED");

            PendingRest(GameService, SessionStatuses::Connected | SessionStatuses::Authenticated | SessionStatuses::CharacterSelected | SessionStatuses::LoggedIn | SessionStatuses::InWorld);

            Sends<AttachFailed>();

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<GameSession> const& GameMessageTable::Get()
{
    static GameRules const table;
    return table;
}
