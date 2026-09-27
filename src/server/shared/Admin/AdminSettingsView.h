/*
 * Project Ambrose by Imjustchico
 * The write half of the settings API every app with live settings answers: PUT /api/settings/<key> takes a value and a reason and answers 422 naming every problem, 409 naming the layer when an environment variable or a command-line override locks the key, and 403 for a restricted key the caller may not change; POST /api/settings/batch applies every entry or none; and GET /api/settings/<key>/history reads the key's audit rows with who, why and the values before and after, a secret's always masked. A caller relayed by the supervisor is named and narrowed by what the supervisor forwards with its token.
 */

#ifndef AMBROSE_ADMINSETTINGSVIEW_H
#define AMBROSE_ADMINSETTINGSVIEW_H

#include "Settings.h"
#include "Types.h"

#include <string_view>

class AdminRouter;
struct AdminRequest;
struct AdminResponse;

class AdminSettingsView
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr uint32 BatchCost = 10;
    static constexpr std::string_view RestrictedPermission = "settings.edit.restricted";

    AdminSettingsView() = delete;

    static void Register(AdminRouter& router, Settings& settings);
    static AdminResponse Put(AdminRouter const& router, Settings& settings, AdminRequest const& request, std::string_view key);
    static AdminResponse Batch(AdminRouter const& router, Settings& settings, AdminRequest const& request);
    static AdminResponse History(Settings const& settings, std::string_view key);
    static SettingAuthor AuthorOf(AdminRequest const& request);
    static int StatusOf(SettingResult result) noexcept;
};

#endif
