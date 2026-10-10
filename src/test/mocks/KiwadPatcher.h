/*
 * Project Ambrose by Imjustchico
 * Replaces one entry of a KIWAD file on disk by appending its new data deflated and pointing the entry's record at it, for tests that edit a copy of an install's archive.
 */

#ifndef AMBROSE_KIWADPATCHER_H
#define AMBROSE_KIWADPATCHER_H

#include "Types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class KiwadPatcher
{
public:
    KiwadPatcher() = delete;

    static bool Replace(std::filesystem::path const& archive, std::string_view name, std::vector<uint8> const& data, std::string& error);
};

#endif
