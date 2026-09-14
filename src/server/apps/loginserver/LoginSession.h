/*
 * Project Ambrose by Imjustchico
 * The login server's bootstrap session: completes the handshake and logs every client message by protocol and name.
 */

#ifndef AMBROSE_LOGINSESSION_H
#define AMBROSE_LOGINSESSION_H

#include "SessionBase.h"

class LoginSession : public SessionBase
{
public:
    using SessionBase::SessionBase;

protected:
    void OnMessage(DmlMessageData& message) override;
};

#endif
