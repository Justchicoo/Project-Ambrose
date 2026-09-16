/*
 * Project Ambrose by Imjustchico
 * The login server's session: completes the handshake and routes every client message through the login message table to its handler.
 */

#ifndef AMBROSE_LOGINSESSION_H
#define AMBROSE_LOGINSESSION_H

#include "LoginMessages.h"
#include "SessionBase.h"

class LoginSession : public SessionBase
{
public:
    using SessionBase::SessionBase;

    void HandleUserAuthenV3(LoginMessages::UserAuthenV3& message);

protected:
    void OnMessage(DmlMessageData& message) override;
};

#endif
