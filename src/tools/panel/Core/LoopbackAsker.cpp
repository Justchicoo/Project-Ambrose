/*
 * Project Ambrose by Imjustchico
 * Sends one request to the supervisor's admin API on 127.0.0.1 through the AdminClient every app shares, and hands back whether it answered, its status and its body.
 */

#include "LoopbackAsker.h"

#include "AdminClient.h"

AdminAnswer LoopbackAsker::Ask(uint16 port, std::string const& token, std::string_view method, std::string_view path, std::string const& body)
{
    AdminClient const client("127.0.0.1", port, token, false);
    AdminClientRequest request;
    request.Method = std::string(method);
    request.Path = std::string(path);
    request.Body = body;
    AdminClientResponse const answer = client.Send(request, Timeout);
    return { answer.Answered, answer.Status, answer.Body, answer.Error };
}
