/*
 * Project Ambrose by Imjustchico
 * The one zone transfer a wizard may have waiting on its client: a request while one waits is ignored, the client's MSG_ZONETRANSFERACK hands the waiting transfer over to be carried out, its MSG_ZONETRANSFERNACK clears it, and the last transfer sent is kept so MSG_RETRYTELEPORT can send it again.
 */

#ifndef AMBROSE_ZONETRANSFERQUEUE_H
#define AMBROSE_ZONETRANSFERQUEUE_H

#include "PlayerMovement.h"

#include <optional>
#include <string>
#include <utility>

struct ZoneTransfer
{
    std::string Zone;
    std::string ZoneDisplay;
    std::string Location;
    PlayerPosition Place;
};

class ZoneTransferQueue
{
public:
    bool Request(ZoneTransfer transfer)
    {
        if (_waiting || _carrying)
            return false;
        _waiting = std::move(transfer);
        return true;
    }

    std::optional<ZoneTransfer> Ack()
    {
        if (!_waiting)
            return std::nullopt;
        _carrying = true;
        return std::exchange(_waiting, std::nullopt);
    }

    bool Nack() noexcept
    {
        return std::exchange(_waiting, std::nullopt).has_value();
    }

    void Finish() noexcept { _carrying = false; }
    bool Busy() const noexcept { return _waiting.has_value() || _carrying; }
    std::optional<ZoneTransfer> const& Waiting() const noexcept { return _waiting; }

private:
    std::optional<ZoneTransfer> _waiting;
    bool _carrying = false;
};

#endif
