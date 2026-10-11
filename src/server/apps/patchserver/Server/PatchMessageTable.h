/*
 * Project Ambrose by Imjustchico
 * The patch server's message table: which messages it handles, which it does not handle yet, and which only the server sends.
 */

#ifndef AMBROSE_PATCHMESSAGETABLE_H
#define AMBROSE_PATCHMESSAGETABLE_H

#include "MessageHandlerTable.h"
#include "PatchSession.h"

namespace PatchMessageTable
{
    MessageHandlerTable<PatchSession> const& Get();
}

#endif
