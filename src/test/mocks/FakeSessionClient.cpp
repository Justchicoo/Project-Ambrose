/*
 * Project Ambrose by Imjustchico
 * Polls a condition every two milliseconds until it holds or the timeout passes.
 */

#include "FakeSessionClient.h"

#include <thread>

bool WaitForCondition(std::function<bool()> const& condition, std::chrono::milliseconds timeout)
{
    auto const deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition())
    {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}
