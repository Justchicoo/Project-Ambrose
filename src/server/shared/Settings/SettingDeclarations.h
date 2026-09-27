/*
 * Project Ambrose by Imjustchico
 * Every live setting any app declares, in one table: each names the apps that read it and whether it is secret or restricted, so an app declares only its own, doc/config/settings.md is written from the whole table, and which settings are secret is answered from it alone.
 */

#ifndef AMBROSE_SETTINGDECLARATIONS_H
#define AMBROSE_SETTINGDECLARATIONS_H

#include "Settings.h"

#include <string>
#include <string_view>
#include <vector>

namespace SettingDeclarations
{
    std::vector<SettingDeclaration> const& All();
    SettingDeclaration const* Find(std::string_view key);
    bool IsSecret(std::string_view key);
    std::string RenderDocument();
}

#endif
