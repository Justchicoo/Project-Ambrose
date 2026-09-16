/*
 * Project Ambrose by Imjustchico
 * Lists every message of the login server's services once: MSG_USER_AUTHEN_V3 handled before authentication, the other LOGIN client requests with the status each will need, the server's replies refused, and the SYSTEM and EXTENDEDBASE rules every app shares.
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

            Pending(LoginService, "MSG_USER_AUTHEN", SessionStatuses::Connected);
            Pending(LoginService, "MSG_USER_AUTHEN_V2", SessionStatuses::Connected);
            Pending(LoginService, "MSG_USER_VALIDATE", SessionStatuses::Connected);
            Pending(LoginService, "MSG_WEB_AUTHEN", SessionStatuses::Connected);
            Pending(LoginService, "MSG_WEB_VALIDATE", SessionStatuses::Connected);
            Pending(LoginService, "MSG_REQUESTCHARACTERLIST", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_REQUESTSERVERLIST", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_CREATECHARACTER", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_DELETECHARACTER", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_SELECTCHARACTER", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_CHANGECHARACTERNAME", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_SAVECHARACTER", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_LOGINLOGCHARACTERCREATION", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_FULFILLPROMOCODE", SessionStatuses::Authenticated);
            Pending(LoginService, "MSG_LOGIN_NOT_AFK", SessionStatuses::Authenticated | SessionStatuses::CharacterSelected);

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

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<LoginSession> const& LoginMessageTable::Get()
{
    static LoginRules const table;
    return table;
}
