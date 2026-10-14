/*
 * Project Ambrose by Imjustchico
 * Answers the Studio window's paths: the summary, a page of quests or cinematics by search, offset and limit, one quest by name and one cinematic by name, a missing one as 404 and a message that is not an object as an empty answer the bridge ignores.
 */

#include "StudioChannel.h"

std::string StudioChannel::Answer(std::string const& message) const
{
    nlohmann::ordered_json const asked = nlohmann::ordered_json::parse(message, nullptr, false);
    if (!asked.is_object())
        return std::string();

    nlohmann::ordered_json const body = asked.contains("body") && asked["body"].is_object() ? asked["body"] : nlohmann::ordered_json::object();
    std::string const path = asked.value("path", std::string());
    std::string const search = body.value("search", std::string());
    std::string const name = body.value("name", std::string());
    std::size_t const offset = body.value("offset", std::size_t{ 0 });
    std::size_t const limit = body.value("limit", std::size_t{ 0 });

    nlohmann::ordered_json reply;
    reply["id"] = asked.value("id", 0);
    auto const answered = [&reply](int status, nlohmann::ordered_json answer)
    {
        reply["status"] = status;
        reply["ok"] = status == 200;
        reply["body"] = std::move(answer);
        return reply.dump();
    };

    if (path == "/studio/summary")
        return answered(200, _content.Summary());
    if (path == "/studio/quests")
        return answered(200, _content.Quests(search, offset, limit));
    if (path == "/studio/cinematics")
        return answered(200, _content.Cinematics(search, offset, limit));
    if (path == "/studio/quest")
    {
        if (std::optional<nlohmann::ordered_json> quest = _content.Quest(name))
            return answered(200, std::move(*quest));
        return answered(404, { { "error", "no quest is named " + name } });
    }
    if (path == "/studio/cinematic")
    {
        std::string error;
        if (std::optional<nlohmann::ordered_json> cinematic = _content.Cinematic(name, error))
            return answered(200, std::move(*cinematic));
        return answered(404, { { "error", error.empty() ? "no cinematic is named " + name : error } });
    }
    return answered(404, { { "error", "the Studio has no path " + path } });
}
