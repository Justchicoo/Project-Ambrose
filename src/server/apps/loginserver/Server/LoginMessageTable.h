/*
 * Project Ambrose by Imjustchico
 * The login server's message table: which messages it handles, which it does not handle yet, and which only the server sends.
 */

#ifndef AMBROSE_LOGINMESSAGETABLE_H
#define AMBROSE_LOGINMESSAGETABLE_H

#include "LoginSession.h"
#include "MessageHandlerTable.h"

namespace LoginMessageTable
{
    MessageHandlerTable<LoginSession> const& Get();
}

#endif
