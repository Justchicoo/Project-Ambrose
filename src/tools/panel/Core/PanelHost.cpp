/*
 * Project Ambrose by Imjustchico
 * Reads a host channel message, routes it by method and path, and writes the reply the bridge in packages/ui expects. A pinned entry is probed before it opens, so a changed certificate refuses the open with both fingerprints and the panel is sent nothing. This computer opens through a fresh local link, and a paired panel through its pairing link, once, straight after pairing, since the program keeps no token. No route takes or returns a password.
 */

#include "PanelHost.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <optional>

namespace
{
    nlohmann::json Problem(std::string const& code, std::string const& message)
    {
        return { { "error", code }, { "message", message } };
    }

    nlohmann::json Described(PanelEntry const& entry)
    {
        return {
            { "id", entry.Id },
            { "name", entry.Name },
            { "origin", entry.Address.Origin.Describe() },
            { "trust", std::string(PanelAddress::Name(entry.Address.Trust)) },
            { "pin", PanelAddress::Grouped(entry.Address.Pin) },
            { "warning", entry.Address.Warning() },
            { "last_user", entry.LastUser },
            { "last_opened", entry.LastOpenedMs },
        };
    }

    std::optional<PanelTrust> TrustOf(nlohmann::json const& body)
    {
        return PanelAddress::TrustNamed(body.value("trust", std::string("public")));
    }
}

PanelHost::PanelHost(PanelList& list, PanelTransport& transport, AdminAsker& asker, std::filesystem::path dataFolder, PanelHostBuild build, Opener open, Clock now,
    uint16 adminPort)
    : _list(list), _transport(transport), _asker(asker), _dataFolder(std::move(dataFolder)), _build(std::move(build)), _open(std::move(open)), _now(std::move(now)),
      _adminPort(adminPort)
{
}

std::string PanelHost::Answer(std::string const& message)
{
    nlohmann::json const asked = nlohmann::json::parse(message, nullptr, false);
    if (!asked.is_object())
        return std::string();
    int status = 200;
    std::string const body = asked.contains("body") ? asked["body"].dump() : std::string("{}");
    std::string const answered = Handle(asked.value("method", std::string("GET")), asked.value("path", std::string()), body, status);
    nlohmann::json reply;
    reply["id"] = asked.value("id", 0);
    reply["status"] = status;
    reply["ok"] = status < 400;
    reply["body"] = nlohmann::json::parse(answered, nullptr, false);
    return reply.dump();
}

std::string PanelHost::Handle(std::string const& method, std::string const& path, std::string const& text, int& status)
{
    nlohmann::json const body = nlohmann::json::parse(text, nullptr, false);
    std::string error;
    auto const refuse = [&status](int code, std::string const& name, std::string const& message)
    {
        status = code;
        return Problem(name, message).dump();
    };

    if (method == "GET" && path == "/panels")
    {
        nlohmann::json answer;
        std::optional<std::string> const token = ThisComputer::Token(_dataFolder.parent_path(), error);
        std::optional<LocalSupervisor> const local = token ? ThisComputer::Find(_asker, *token, _adminPort, error) : std::nullopt;
        answer["this_computer"] = { { "found", local.has_value() }, { "revision", local ? local->Revision : std::string() }, { "word", local ? std::string() : error } };
        answer["panels"] = nlohmann::json::array();
        std::string listError;
        for (PanelEntry const& entry : _list.Entries(listError))
            answer["panels"].push_back(Described(entry));
        return answer.dump();
    }

    if (method == "POST" && path == "/panels/probe")
    {
        nlohmann::json answer;
        answer["states"] = nlohmann::json::array();
        for (PanelEntry const& entry : _list.Entries(error))
        {
            PanelProbeResult const probed = PanelProbe::Run(_transport, entry.Address);
            answer["states"].push_back({ { "id", entry.Id }, { "state", std::string(PanelProbe::Name(probed.State)) }, { "word", probed.Word } });
        }
        return answer.dump();
    }

    if (!body.is_object())
        return refuse(400, "invalid", "the request carries no object");

    if (method == "POST" && path == "/panels/inspect")
    {
        std::optional<PanelAddress> const address = PanelAddress::Parse(body.value("address", std::string()), PanelTrust::Public, "", error);
        if (!address)
            return refuse(422, "address_refused", error);
        if (address->Origin.Scheme != "https")
            return nlohmann::json{ { "origin", address->Origin.Describe() }, { "served", "" } }.dump();
        PanelAddress probing = *address;
        probing.Trust = PanelTrust::Pinned;
        PanelReply const reply = _transport.Get(probing, PanelProbe::Path);
        if (!reply.PinRefused)
            return refuse(502, "unreachable", reply.Error.empty() ? std::string("the address answered no certificate to compare") : reply.Error);
        return nlohmann::json{ { "origin", address->Origin.Describe() }, { "served", PanelAddress::Grouped(reply.Served) }, { "fingerprint", reply.Served } }.dump();
    }

    if (method == "POST" && path == "/panels")
    {
        std::optional<PanelTrust> const trust = TrustOf(body);
        std::optional<PanelAddress> const address = trust ? PanelAddress::Parse(body.value("address", std::string()), *trust, body.value("pin", std::string()), error) : std::nullopt;
        if (!address)
            return refuse(422, "address_refused", trust ? error : std::string("the trust named is not one the program knows"));
        std::optional<int64> const id = _list.Add(body.value("name", std::string()), *address, error);
        if (!id)
            return refuse(409, "not_added", error);
        return nlohmann::json{ { "id", *id }, { "warning", address->Warning() } }.dump();
    }

    if (method == "POST" && path == "/panels/pair")
    {
        std::optional<PanelPairing> const pairing = PanelPairing::Parse(body.value("line", std::string()), error);
        if (!pairing)
            return refuse(422, "pairing_refused", error);
        std::string const name = body.value("name", pairing->Address.Origin.Host);
        std::optional<int64> const id = _list.Add(name, pairing->Address, error);
        if (!id)
            return refuse(409, "not_added", error);
        std::optional<PanelEntry> const entry = _list.Find(*id, error);
        if (!entry || !_open(*entry, pairing->Link, error))
            return refuse(502, "not_opened", error);
        _list.Opened(*id, std::string(), _now(), error);
        return nlohmann::json{ { "id", *id } }.dump();
    }

    int64 const id = body.value("id", int64{ -1 });
    if (method == "POST" && path == "/panels/rename")
        return _list.Rename(id, body.value("name", std::string()), error) ? std::string("{}") : refuse(409, "not_renamed", error);

    if (method == "POST" && path == "/panels/edit")
    {
        std::optional<PanelTrust> const trust = TrustOf(body);
        std::optional<PanelAddress> const address = trust ? PanelAddress::Parse(body.value("address", std::string()), *trust, body.value("pin", std::string()), error) : std::nullopt;
        if (!address)
            return refuse(422, "address_refused", error);
        return _list.Edit(id, *address, error) ? std::string("{}") : refuse(409, "not_edited", error);
    }

    if (method == "POST" && path == "/panels/forget")
        return _list.Forget(id, error) ? std::string("{}") : refuse(409, "not_forgotten", error);

    if (method == "POST" && path == "/panels/open")
    {
        if (id == ThisComputerId)
        {
            std::optional<std::string> const token = ThisComputer::Token(_dataFolder.parent_path(), error);
            std::optional<std::string> const link = token ? ThisComputer::LocalLink(_asker, *token, _adminPort, error) : std::nullopt;
            if (!link)
                return refuse(409, "not_opened", error);
            std::optional<ShellOrigin> const origin = ShellOrigin::Of(*link);
            PanelEntry local;
            local.Id = ThisComputerId;
            local.Name = "This computer";
            local.Address.Origin = origin.value_or(ShellOrigin{});
            local.Address.Trust = PanelTrust::Loopback;
            return _open(local, *link, error) ? std::string("{}") : refuse(502, "not_opened", error);
        }
        std::optional<PanelEntry> const entry = _list.Find(id, error);
        if (!entry)
            return refuse(404, "not_listed", error);
        if (entry->Address.Trust == PanelTrust::Pinned)
        {
            PanelProbeResult const probed = PanelProbe::Run(_transport, entry->Address);
            if (probed.State == PanelState::CertificateChanged)
                return refuse(409, "certificate_changed", probed.Word);
        }
        if (!_open(*entry, entry->Address.Url(), error))
            return refuse(502, "not_opened", error);
        _list.Opened(id, entry->LastUser, _now(), error);
        return std::string("{}");
    }

    if (method == "GET" && path == "/about")
    {
        std::optional<std::string> const token = ThisComputer::Token(_dataFolder.parent_path(), error);
        std::optional<LocalSupervisor> const local = token ? ThisComputer::Find(_asker, *token, _adminPort, error) : std::nullopt;
        return nlohmann::json{ { "version", _build.Version }, { "revision", _build.Revision }, { "webview", _build.WebViewVersion },
            { "supervisor_revision", local ? local->Revision : std::string() } }
            .dump();
    }

    return refuse(404, "no_route", fmt::format("the panel program has no {} {}", method, path));
}
