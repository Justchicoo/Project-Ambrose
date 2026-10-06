/*
 * Project Ambrose by Imjustchico
 * Makes one allocation of AMBROSE_PROBE_SIZE bytes under the fuzz allocation ceiling, proving the ceiling trips just above the frame limit.
 */

#include "AllocationCeiling.h"

#include <cstddef>
#include <cstdint>
#include <memory>

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    AllocationCeiling::Install();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t const*, std::size_t)
{
    AllocationCeiling::Scope const ceiling;
    std::unique_ptr<uint8_t[]> const block(new uint8_t[AMBROSE_PROBE_SIZE]);
    static_cast<uint8_t volatile*>(block.get())[0] = 1;
    return 0;
}
