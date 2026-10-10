/*
 * Project Ambrose by Imjustchico
 * Reads the Patch options from config, trimming the public address and dropping its trailing slashes, refusing one that is not an http or https URL, refusing a revision name that could leave the output folder, and reading the supported locale list without its empty entries.
 */

#include "PatchSettings.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

bool PatchSettings::SupportsLocale(std::string_view locale) const
{
    return std::any_of(SupportedLocales.begin(), SupportedLocales.end(), [locale](std::string const& supported) { return Ambrose::EqualsIgnoreCase(supported, locale); });
}

PatchSettings PatchSettings::Load(ConfigMgr const& config, std::vector<std::string>* problems)
{
    auto report = [problems](std::string problem)
    {
        if (problems)
            problems->push_back(std::move(problem));
    };

    PatchSettings settings;
    std::string url(Ambrose::Trim(config.GetOption<std::string>("Http.PublicBaseUrl", std::string(DefaultPublicBaseUrl), true)));
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    if (!url.starts_with("http://") && !url.starts_with("https://"))
        report(fmt::format("Http.PublicBaseUrl = '{}' is not an http:// or https:// address; using {}", url, DefaultPublicBaseUrl));
    else if (url.size() <= std::string_view("https://").size() || url.find_first_of(" \t\r\n") != std::string::npos)
        report(fmt::format("Http.PublicBaseUrl = '{}' names no host; using {}", url, DefaultPublicBaseUrl));
    else
        settings.PublicBaseUrl = std::move(url);

    std::string const output(Ambrose::Trim(config.GetOption<std::string>("Patch.OutputDir", "", true)));
    if (!output.empty())
        settings.OutputDir = ConfigMgr::PathFromUtf8(output);

    std::string const revision(Ambrose::Trim(config.GetOption<std::string>("Patch.Revision", "", true)));
    if (revision.find_first_of("/\\:") != std::string::npos || revision == "." || revision == "..")
        report(fmt::format("Patch.Revision = '{}' must be a revision name such as r806919.Wizard_1_610, not a path; the only revision in Patch.OutputDir is served", revision));
    else
        settings.Revision = revision.starts_with("V_") ? revision.substr(2) : revision;

    std::string const localeList = config.GetOption<std::string>("Patch.SupportedLocales", std::string(DefaultLocales), true);
    std::vector<std::string> locales;
    for (std::string_view const locale : Ambrose::Tokenize(localeList, ',', false))
        if (std::string_view const trimmed = Ambrose::Trim(locale); !trimmed.empty())
            locales.emplace_back(trimmed);
    if (locales.empty())
        report(fmt::format("Patch.SupportedLocales lists no locale; using {}", DefaultLocales));
    else
        settings.SupportedLocales = std::move(locales);
    return settings;
}
