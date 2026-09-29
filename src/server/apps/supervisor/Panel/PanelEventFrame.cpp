/*
 * Project Ambrose by Imjustchico
 * Reads a client's frame into its parts with the reason it was refused when it cannot be read, and writes the server's frames with fmt around data that is already JSON, quoting every text through the JSON library so a name or an id can never break out of its string.
 */

#include "PanelEventFrame.h"
#include "PanelEventCatalog.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <utility>

namespace
{
    bool Printable(std::string const& text)
    {
        return std::all_of(text.begin(), text.end(), [](char character) { return character >= 0x20 && character <= 0x7E; });
    }
}

std::optional<PanelIncomingFrame> PanelEventFrame::Read(std::string const& text, bool binary, std::string& problem)
{
    if (binary)
    {
        problem = "the panel's event socket takes text frames only";
        return std::nullopt;
    }
    nlohmann::json document = nlohmann::json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        problem = "a frame is one JSON object";
        return std::nullopt;
    }

    auto const version = document.find("v");
    if (version == document.end() || !version->is_number_integer())
    {
        problem = "a frame names its protocol version in v";
        return std::nullopt;
    }
    if (!version->is_number_unsigned() || version->get<uint64>() != PanelEventCatalog::Version)
    {
        problem = fmt::format("protocol version {} is not spoken here; this panel speaks {}", version->dump(), PanelEventCatalog::Version);
        return std::nullopt;
    }

    PanelIncomingFrame frame;
    auto const type = document.find("type");
    if (type == document.end() || !type->is_string() || type->get_ref<std::string const&>().empty() || type->get_ref<std::string const&>().size() > MaxNameLength)
    {
        problem = "a frame names its type in type, as text of at most 64 characters";
        return std::nullopt;
    }
    frame.Type = type->get<std::string>();

    if (auto const id = document.find("id"); id != document.end() && !id->is_null())
    {
        if (!id->is_string() || id->get_ref<std::string const&>().empty() || id->get_ref<std::string const&>().size() > MaxIdLength || !Printable(id->get_ref<std::string const&>()))
        {
            problem = "id is a request id of 1 to 64 printable characters";
            return std::nullopt;
        }
        frame.Id = id->get<std::string>();
    }

    if (auto const scope = document.find("scope"); scope != document.end() && !scope->is_null())
    {
        if (!scope->is_object())
        {
            problem = "scope is an object naming an app, or null for the whole panel";
            return std::nullopt;
        }
        for (auto entry = scope->begin(); entry != scope->end(); ++entry)
        {
            if (entry.key() != "app")
            {
                problem = fmt::format("scope names an app and nothing else in protocol version {}, not {}", PanelEventCatalog::Version, Ambrose::ForLog(entry.key(), 64));
                return std::nullopt;
            }
            if (!entry.value().is_string() || entry.value().get_ref<std::string const&>().empty() || entry.value().get_ref<std::string const&>().size() > MaxNameLength)
            {
                problem = "scope.app is an app's name";
                return std::nullopt;
            }
            frame.App = entry.value().get<std::string>();
        }
    }

    if (auto const seq = document.find("seq"); seq != document.end() && !seq->is_null())
    {
        if (!seq->is_number_unsigned())
        {
            problem = "seq is the last sequence number the page saw, a whole number from 0";
            return std::nullopt;
        }
        frame.Seq = seq->get<uint64>();
    }

    if (auto const data = document.find("data"); data != document.end())
    {
        if (!data->is_object())
        {
            problem = "data is a JSON object, never text holding one";
            return std::nullopt;
        }
        frame.Data = std::move(*data);
    }
    return frame;
}

std::string PanelEventFrame::Quoted(std::string_view text)
{
    return nlohmann::json(std::string(text)).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string PanelEventFrame::Write(std::string_view type, std::optional<std::string> const& id, std::string_view app, std::optional<uint64> seq, int64 timeMs, std::string_view data)
{
    std::string const scope = app.empty() ? std::string("null") : fmt::format(R"({{"app":{}}})", Quoted(app));
    return fmt::format(R"({{"v":{},"type":{},"id":{},"scope":{},"seq":{},"time":{},"data":{}}})", PanelEventCatalog::Version, Quoted(type),
        id ? Quoted(*id) : std::string("null"), scope, seq ? std::to_string(*seq) : std::string("null"), timeMs, data.empty() ? std::string_view("{}") : data);
}

std::string PanelEventFrame::DroppedData(std::string_view stream, uint64 first, uint64 last, uint64 count)
{
    return fmt::format(R"({{"stream":{},"count":{},"first":{},"last":{}}})", Quoted(stream), count, first, last);
}

int64 PanelEventFrame::NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
