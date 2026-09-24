/*
 * Project Ambrose by Imjustchico
 * The session values the login hashes mix in: the session id and the SessionOffer seconds and milliseconds, written as one decimal string with no separators.
 */

#ifndef AMBROSE_LOGINSALT_H
#define AMBROSE_LOGINSALT_H

#include "Types.h"

#include <string>

struct LoginSalt
{
    uint16 SessionId = 0;
    uint32 Seconds = 0;
    uint32 Milliseconds = 0;

    std::string ToString() const;

    bool operator==(LoginSalt const&) const = default;
};

#endif
