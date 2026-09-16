/*
 * Project Ambrose by Imjustchico
 * Wipes secret bytes through Botan's scrub routine.
 */

#include "SecureMemory.h"

#include <botan/mem_ops.h>

void Ambrose::Crypto::SecureWipe(std::span<uint8> bytes) noexcept
{
    if (!bytes.empty())
        Botan::secure_scrub_memory(bytes.data(), bytes.size());
}
