/*
 * Project Ambrose by Imjustchico
 * Fills any registered message with deterministic pseudo-random values and checks encode, decode, size, truncation, and trailing-byte handling.
 */

#ifndef AMBROSE_MESSAGEROUNDTRIP_H
#define AMBROSE_MESSAGEROUNDTRIP_H

#include "DynamicMessage.h"

#include <string>

namespace MessageRoundTrip
{
    uint64 SeedFor(MessageInfo const& info) noexcept;
    DynamicMessage MakeRandom(MessageCatalogPtr const& catalog, MessageInfo const& info, uint64 seed);
    bool SameValue(DmlValue const& left, DmlValue const& right) noexcept;
    std::string Check(MessageCatalogPtr const& catalog, MessageInfo const& info, uint64 seed);
}

#endif
