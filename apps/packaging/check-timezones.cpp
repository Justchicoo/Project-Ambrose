/*
 * Project Ambrose by Imjustchico
 * Fails the runtime image build unless libstdc++ loads the system tzdata version and resolves every scheduled time zone.
 */

#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

int main()
{
    constexpr std::string_view VersionPrefix = "# version ";
    std::ifstream systemDatabase("/usr/share/zoneinfo/tzdata.zi");
    std::string versionLine;
    if (!systemDatabase || !std::getline(systemDatabase, versionLine) || !versionLine.starts_with(VersionPrefix))
    {
        std::cerr << "time-zone check: /usr/share/zoneinfo/tzdata.zi is missing or has no version header\n";
        return 1;
    }

    std::string const systemVersion = versionLine.substr(VersionPrefix.size());
    std::string const libraryVersion = std::chrono::get_tzdb().version;
    if (libraryVersion != systemVersion)
    {
        std::cerr << "time-zone check: libstdc++ loaded " << libraryVersion << " but the system database is " << systemVersion << '\n';
        return 1;
    }

    for (std::string_view const name : { "America/New_York", "Europe/London", "Australia/Lord_Howe" })
    {
        try
        {
            std::chrono::locate_zone(name);
        }
        catch (std::exception const& error)
        {
            std::cerr << "time-zone check: " << name << " could not be resolved: " << error.what() << '\n';
            return 1;
        }
    }

    std::cout << "time-zone check: libstdc++ resolved all required zones from system tzdata " << systemVersion << '\n';
    return 0;
}
