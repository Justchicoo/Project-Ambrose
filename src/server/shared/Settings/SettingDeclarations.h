/*
 * Project Ambrose by Imjustchico
 * Every live setting any app declares, in one table: each names the apps that read it, so an app declares only its own and doc/config/settings.md is written from the whole table.
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
    std::string RenderDocument();
}

#endif
