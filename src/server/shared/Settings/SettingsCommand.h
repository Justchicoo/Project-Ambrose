/*
 * Project Ambrose by Imjustchico
 * The settings command every app's console and the game server's .settings share: list, get, set, reset and history, each answering in lines of text, with who ran it and where it came in carried into the audit.
 */

#ifndef AMBROSE_SETTINGSCOMMAND_H
#define AMBROSE_SETTINGSCOMMAND_H

#include "Settings.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace SettingsCommand
{
    using Reply = std::function<void(std::string_view)>;

    inline constexpr std::string_view Arguments = "[list [category] | get <key> | set <key> <value> [reason] | reset <key> [reason] | history <key>]";
    inline constexpr std::string_view Help = "list, read and change live settings, and read what changed them";

    bool Run(Settings& settings, std::vector<std::string> const& arguments, SettingAuthor const& author, Reply const& reply);
}

#endif
