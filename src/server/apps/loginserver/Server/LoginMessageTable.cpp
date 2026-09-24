/*
 * Project Ambrose by Imjustchico
 * Lists every message of the login server's services once: MSG_USER_AUTHEN_V3 and the older authentication messages handled before authentication, MSG_REQUESTCHARACTERLIST, MSG_REQUESTSERVERLIST and MSG_SELECTCHARACTER handled once authenticated, MSG_LOGIN_NOT_AFK handled from connection through character selection, the other LOGIN client requests with the status each will need, the server's replies refused and the ones it sends declared, and the SYSTEM and EXTENDEDBASE rules every app shares.
 */

#include "LoginMessageTable.h"
#include "SystemMessageRules.h"

namespace
{
    using namespace LoginMessages;
    using SystemMessages::ExtendedBaseService;
    using SystemMessages::SystemService;

    class LoginRules : public MessageHandlerTable<LoginSession>
    {
    public:
        LoginRules() : MessageHandlerTable<LoginSession>("loginserver", { SystemService, ExtendedBaseService, LoginService })
        {
            Accept<&LoginSession::HandleUserAuthenV3>(SessionStatuses::Connected, MessageProcessing::InPlace, "LoginSession::HandleUserAuthenV3");
            Accept<&LoginSession::HandleUserAuthen>(SessionStatuses::Connected, MessageProcessing::InPlace, "LoginSession::HandleUserAuthen");
            Accept<&LoginSession::HandleUserAuthenV2>(SessionStatuses::Connected, MessageProcessing::InPlace, "LoginSession::HandleUserAuthenV2");
            Accept<&LoginSession::HandleWebAuthen>(SessionStatuses::Connected, MessageProcessing::InPlace, "LoginSession::HandleWebAuthen");
            Accept<&LoginSession::HandleWebValidate>(SessionStatuses::Connected, MessageProcessing::InPlace, "LoginSession::HandleWebValidate");
            Accept<&LoginSession::HandleRequestCharacterList>(SessionStatuses::Authenticated, MessageProcessing::InPlace, "LoginSession::HandleRequestCharacterList");
            Accept<&LoginSession::HandleRequestServerList>(SessionStatuses::Authenticated, MessageProcessing::InPlace, "LoginSession::HandleRequestServerList");
            Accept<&LoginSession::HandleSelectCharacter>(SessionStatuses::Authenticated, MessageProcessing::InPlace, "LoginSession::HandleSelectCharacter");
            Accept<&LoginSession::HandleLoginNotAfk>(SessionStatuses::Connected | SessionStatuses::Authenticated | SessionStatuses::CharacterSelected, MessageProcessing::InPlace, "LoginSession::HandleLoginNotAfk");

            Pending(LoginService, "MSG_USER_VALIDATE", SessionStatuses::Connected);
            Pending(LoginService, "MSG_CREATECHARACTER", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_DELETECHARACTER", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_CHANGECHARACTERNAME", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_SAVECHARACTER", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_LOGINLOGCHARACTERCREATION", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_FULFILLPROMOCODE", SessionStatuses::Authenticated);

            Refuse(LoginService, "MSG_CHARACTERINFO");
            Refuse(LoginService, "MSG_CHARACTERLIST");
            Refuse(LoginService, "MSG_CHARACTERSELECTED");
            Refuse(LoginService, "MSG_CREATECHARACTERRESPONSE");
            Refuse(LoginService, "MSG_DELETECHARACTERRESPONSE");
            Refuse(LoginService, "MSG_SERVERLIST");
            Refuse(LoginService, "MSG_STARTCHARACTERLIST");
            Refuse(LoginService, "MSG_USER_AUTHEN_RSP");
            Refuse(LoginService, "MSG_USER_VALIDATE_RSP");
            Refuse(LoginService, "MSG_USER_ADMIT_IND");
            Refuse(LoginService, "MSG_DISCONNECT_LOGIN_AFK");
            Refuse(LoginService, "MSG_LOGINSERVERSHUTDOWN");
            Refuse(LoginService, "MSG_WEBCHARACTERINFO");

            Sends<UserAuthenRsp>();
            Sends<UserAdmitInd>();
            Sends<DisconnectLoginAfk>();
            Sends<LoginServerShutdown>();
            Sends<StartCharacterList>();
            Sends<CharacterInfo>();
            Sends<CharacterList>();
            Sends<ServerList>();
            Sends<CharacterSelected>();

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<LoginSession> const& LoginMessageTable::Get()
{
    static LoginRules const table;
    return table;
}
