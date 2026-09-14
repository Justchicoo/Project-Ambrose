/*
 * Project Ambrose by Imjustchico
 * Looks up each client message in the live message registry and logs its protocol, tag, service and order, or its raw ids when the registry does not know it.
 */

#include "LoginSession.h"
#include "Log.h"
#include "MessageRegistry.h"

void LoginSession::OnMessage(DmlMessageData& message)
{
    MessageInfoPtr const info = sMessageRegistry.Find(message.ServiceId, message.Order);
    if (info && info->Protocol && info->Definition)
        LOG_INFO("network.opcode", "{} {} ({}:{}) from session {}, {} bytes", info->Protocol->ProtocolType, info->Definition->Tag, message.ServiceId, message.Order, GetSessionId(), message.Body.size());
    else
        LOG_INFO("network.opcode", "Unknown message ({}:{}) from session {}, {} bytes", message.ServiceId, message.Order, GetSessionId(), message.Body.size());
}
