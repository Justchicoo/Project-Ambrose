/*
 * Project Ambrose by Imjustchico
 * The Patch options the patch server's sessions and manifest read: the public address its manifest is fetched from, the folder the patch list generator writes, the revision served from it and the locales it supports, loaded from config with each problem reported.
 */

#ifndef AMBROSE_PATCHSETTINGS_H
#define AMBROSE_PATCHSETTINGS_H

#include "Types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

struct PatchSettings
{
    static constexpr std::string_view DefaultPublicBaseUrl = "http://127.0.0.1:12501";
    static constexpr std::string_view DefaultLocales = "English";

    std::string PublicBaseUrl = std::string(DefaultPublicBaseUrl);
    std::filesystem::path OutputDir;
    std::string Revision;
    std::vector<std::string> SupportedLocales{ std::string(DefaultLocales) };

    bool SupportsLocale(std::string_view locale) const;

    static PatchSettings Load(ConfigMgr const& config, std::vector<std::string>* problems = nullptr);

    bool operator==(PatchSettings const&) const = default;
};

#endif
