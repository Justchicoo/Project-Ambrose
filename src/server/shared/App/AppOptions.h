/*
 * Project Ambrose by Imjustchico
 * Command-line options every server app accepts: config file, version, help, and repeatable setting overrides.
 */

#ifndef AMBROSE_APPOPTIONS_H
#define AMBROSE_APPOPTIONS_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct AppOptions
{
    std::string ConfigFile;
    bool ShowVersion = false;
    bool ShowHelp = false;
    std::vector<std::pair<std::string, std::string>> Overrides;
    std::string Error;

    static AppOptions Parse(std::vector<std::string> const& arguments, std::string_view defaultConfigFile);
    static std::string Usage(std::string_view appName, std::string_view defaultConfigFile);
};

#endif
