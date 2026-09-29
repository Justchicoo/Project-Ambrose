/*
 * Project Ambrose by Imjustchico
 * The envelope every frame on the panel's event socket travels in. Reading one refuses a binary frame, anything that is not one JSON object, a protocol version other than the one spoken here, a type that is not text, a request id that is not one to sixty-four printable characters, a scope that names anything but an app, a sequence that is not a whole number and data that is not an object, each with the sentence the socket closes with; envelope keys it does not know are ignored so a newer page still reads. Writing one always sets all seven keys in the same order, with the request id echoed or null, the app scope or null for the whole panel, the sequence only on a stream record, the time in Unix milliseconds and the data as an object, never as text.
 */

#ifndef AMBROSE_PANELEVENTFRAME_H
#define AMBROSE_PANELEVENTFRAME_H

#include "Types.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

struct PanelIncomingFrame
{
    std::string Type = {};
    std::optional<std::string> Id = {};
    std::optional<std::string> App = {};
    std::optional<uint64> Seq = {};
    nlohmann::json Data = nlohmann::json::object();
};

class PanelEventFrame
{
public:
    static constexpr std::size_t MaxIdLength = 64;
    static constexpr std::size_t MaxNameLength = 64;

    PanelEventFrame() = delete;

    static std::optional<PanelIncomingFrame> Read(std::string const& text, bool binary, std::string& problem);
    static std::string Write(std::string_view type, std::optional<std::string> const& id, std::string_view app, std::optional<uint64> seq, int64 timeMs, std::string_view data);
    static std::string DroppedData(std::string_view stream, uint64 first, uint64 last, uint64 count);
    static std::string Quoted(std::string_view text);
    static int64 NowMs();
};

#endif
