/*
 * Project Ambrose by Imjustchico
 * Parses -c/--config, -v/--version, -h/--help, and --set Key=Value by hand, trimming and unquoting values, and reports the first unusable argument.
 */

#include "AppOptions.h"
#include "StringUtil.h"

#include <fmt/format.h>

AppOptions AppOptions::Parse(std::vector<std::string> const& arguments, std::string_view defaultConfigFile)
{
    AppOptions options;
    options.ConfigFile = std::string(defaultConfigFile);
    auto takeValue = [&](std::size_t& index, std::string_view name, std::string& value) -> bool
    {
        std::string_view const argument = arguments[index];
        if (std::size_t const equals = argument.find('='); equals != std::string_view::npos)
        {
            value = std::string(argument.substr(equals + 1));
            return true;
        }
        if (index + 1 >= arguments.size())
        {
            options.Error = fmt::format("option '{}' needs a value", name);
            return false;
        }
        value = arguments[++index];
        return true;
    };

    for (std::size_t i = 1; i < arguments.size() && options.Error.empty(); ++i)
    {
        std::string_view const argument = arguments[i];
        std::string_view const name = argument.substr(0, argument.find('='));
        if (argument == "-v" || argument == "--version")
            options.ShowVersion = true;
        else if (argument == "-h" || argument == "--help" || argument == "-?")
            options.ShowHelp = true;
        else if (argument == "-c" || name == "--config")
        {
            std::string value;
            if (takeValue(i, name, value))
            {
                if (value.empty())
                    options.Error = "option '--config' needs a file name";
                else
                    options.ConfigFile = value;
            }
        }
        else if (name == "--set")
        {
            std::string value;
            if (!takeValue(i, name, value))
                break;
            std::size_t const equals = value.find('=');
            std::string_view const key = equals == std::string::npos ? std::string_view() : Ambrose::Trim(std::string_view(value).substr(0, equals));
            if (key.empty())
                options.Error = fmt::format("option '--set' needs Key=Value, got '{}'", value);
            else
            {
                std::string_view setting = Ambrose::Trim(std::string_view(value).substr(equals + 1));
                if (setting.size() >= 2 && setting.front() == '"' && setting.back() == '"')
                    setting = setting.substr(1, setting.size() - 2);
                options.Overrides.emplace_back(std::string(key), std::string(setting));
            }
        }
        else
            options.Error = fmt::format("unknown argument '{}'", argument);
    }
    return options;
}

std::string AppOptions::Usage(std::string_view appName, std::string_view defaultConfigFile)
{
    return fmt::format(
        "Usage: {0} [options]\n"
        "  -c, --config <file>   configuration file (default {1})\n"
        "  --set <Key=Value>     override one option, may repeat\n"
        "  -v, --version         print the version and exit\n"
        "  -h, --help            print this help and exit\n",
        appName, defaultConfigFile);
}
