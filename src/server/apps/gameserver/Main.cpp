/*
 * Project Ambrose by Imjustchico
 * Game server entry point: prints the startup banner and exits.
 */

#include "Banner.h"

#include <fmt/core.h>

#include <cstdlib>
#include <string_view>

int main()
{
    Ambrose::Banner::Show("gameserver", [](std::string_view line) { fmt::print("{}\n", line); });
    return EXIT_SUCCESS;
}
