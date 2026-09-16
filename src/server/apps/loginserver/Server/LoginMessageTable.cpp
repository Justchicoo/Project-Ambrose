/*
 * Project Ambrose by Imjustchico
 * Lists every message of the login server's services once: MSG_USER_AUTHEN_V3 handled before authentication, the other LOGIN client requests with the status each will need, the server's replies refused, SYSTEM pings and the EXTENDEDBASE record messages not handled yet, and the server-only EXTENDEDBASE messages refused.
 */

#include "LoginMessageTable.h"

namespace
{
    using namespace LoginMessages;

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

            Pending(SystemService, "MSG_PING", SessionStatuses::Any);
            Pending(SystemService, "MSG_PING_RSP", SessionStatuses::Any);

            Pending(ExtendedBaseService, "MSG_RAW_TEXT", SessionStatuses::Any);
            Pending(ExtendedBaseService, "MSG_CUSTOMDICT", SessionStatuses::Any);
            Pending(ExtendedBaseService, "MSG_CUSTOMRECORD", SessionStatuses::Any);
            Pending(ExtendedBaseService, "MSG_RAWRECORD", SessionStatuses::Any);
            Refuse(ExtendedBaseService, "MSG_SERVERMESSAGE");
            Refuse(ExtendedBaseService, "MSG_FORCE_DISCONNECT");
        }
    };
}

MessageHandlerTable<LoginSession> const& LoginMessageTable::Get()
{
    static LoginRules const table;
    return table;
}
