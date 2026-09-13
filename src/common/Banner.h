/*
 * Project Ambrose by Imjustchico
 * Startup banner shown by every app, written through a caller-supplied line sink.
 */

#ifndef AMBROSE_BANNER_H
#define AMBROSE_BANNER_H

#include <functional>
#include <string_view>

namespace Ambrose::Banner
{
    void Show(std::string_view appName, std::function<void(std::string_view)> const& log);
}

#endif
