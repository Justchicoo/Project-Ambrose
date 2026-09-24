/*
 * Project Ambrose by Imjustchico
 * Counting the tick down to the next beat. The first beat is written as soon as the server is configured rather than one interval later, so a realm is reachable from the moment it can serve rather than after a wait no player can see the reason for, and stopping marks the realm offline rather than erasing when it last beat, so the list is right the moment the server goes while when it was last alive stays true.
 */

#include "RealmHeartbeat.h"

#include "ConfigMgr.h"
#include "Log.h"
#include "StringUtil.h"

#include <chrono>

#include <utility>

RealmHeartbeatSettings RealmHeartbeatSettings::Load(ConfigMgr const& config)
{
    RealmHeartbeatSettings settings;
    settings.RealmName = std::string(Ambrose::Trim(config.GetOption<std::string>("Realm.Name", "", true)));
    settings.Address = std::string(Ambrose::Trim(config.GetOption<std::string>("Realm.Address", "", true)));
    if (settings.Address.empty())
        settings.Address = std::string(Ambrose::Trim(config.GetOption<std::string>("PublicAddress", "", true)));
    if (settings.Address.empty())
        settings.Address = std::string(Ambrose::Trim(config.GetOption<std::string>("BindIP", "127.0.0.1", true)));
    if (settings.Address.empty() || settings.Address == "0.0.0.0" || settings.Address == "::")
        settings.Address = "127.0.0.1";
    settings.Port = static_cast<uint16>(config.GetOption<uint32>("WorldServerPort", 12333, true));
    uint32 const interval = config.GetOption<uint32>("Realm.HeartbeatInterval", DefaultIntervalSeconds, true);
    settings.IntervalSeconds = interval != 0 ? interval : DefaultIntervalSeconds;
    return settings;
}

void RealmHeartbeat::Configure(RealmHeartbeatSettings settings, Writer writer, Counter counter, Registrar registrar, Ready canWrite)
{
    _settings = std::move(settings);
    _writer = std::move(writer);
    _counter = std::move(counter);
    _registrar = std::move(registrar);
    _canWrite = std::move(canWrite);
    _since = std::chrono::milliseconds::zero();
    _beats = 0;

    if (_settings.RealmName.empty() || !_writer)
    {
        _beating = false;
        LOG_INFO("server.worldserver", "Realm.Name names no realm, so this server does not appear in the realm list and no player is sent to it");
        return;
    }
    if (_canWrite && !_canWrite())
    {
        _beating = false;
        LOG_INFO("server.worldserver", "There is no login database to keep the realm list in, so realm {} is not offered to players", _settings.RealmName);
        return;
    }

    _beating = true;
    if (_registrar)
        _registrar(_settings);
    LOG_INFO("server.worldserver", "Realm {} at {}:{} says it is alive every {} second(s)", _settings.RealmName, _settings.Address, _settings.Port, _settings.IntervalSeconds);
    BeatNow();
}

void RealmHeartbeat::BeatNow()
{
    if (!_beating || !_writer)
        return;
    uint32 const population = _counter ? _counter() : 0;
    int64 const now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    _writer(_settings.RealmName, population, now, true);
    _since = std::chrono::milliseconds::zero();
    ++_beats;
}

void RealmHeartbeat::Update(std::chrono::milliseconds diff)
{
    if (!_beating)
        return;
    _since += diff;
    if (_since < std::chrono::seconds(_settings.IntervalSeconds))
        return;
    BeatNow();
}

void RealmHeartbeat::Stop()
{
    if (!_beating)
        return;
    if (_writer)
        _writer(_settings.RealmName, 0, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), false);
    _beating = false;
}
