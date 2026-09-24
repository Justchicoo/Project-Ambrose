/*
 * Project Ambrose by Imjustchico
 * The SYSTEM and EXTENDEDBASE rules every app's message table shares: pings answered in place, the record messages not handled yet, the server-only messages refused, and the base messages sessions send declared.
 */

#ifndef AMBROSE_SYSTEMMESSAGERULES_H
#define AMBROSE_SYSTEMMESSAGERULES_H

#include "MessageHandlerTable.h"
#include "SessionBase.h"
#include "SystemMessages.h"

namespace SystemMessages
{
    template<typename SessionT>
    void AddRules(MessageHandlerTable<SessionT>& table)
    {
        table.template Accept<&SessionBase::HandlePing>(SessionStatuses::Any, MessageProcessing::InPlace, "SessionBase::HandlePing");
        table.Pending(SystemService, "MSG_PING_RSP", SessionStatuses::Any);

        table.Pending(ExtendedBaseService, "MSG_RAW_TEXT", SessionStatuses::Any);
        table.Pending(ExtendedBaseService, "MSG_CUSTOMDICT", SessionStatuses::Any);
        table.Pending(ExtendedBaseService, "MSG_CUSTOMRECORD", SessionStatuses::Any);
        table.Pending(ExtendedBaseService, "MSG_RAWRECORD", SessionStatuses::Any);
        table.Refuse(ExtendedBaseService, "MSG_SERVERMESSAGE");
        table.Refuse(ExtendedBaseService, "MSG_FORCE_DISCONNECT");

        table.template Sends<PingRsp>();
        table.template Sends<ServerMessage>();
        table.template Sends<ForceDisconnect>();
    }
}

#endif
