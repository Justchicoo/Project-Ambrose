/*
 * Project Ambrose by Imjustchico
 * The one catalog of what an operator may be allowed to do: every permission the panel knows, in the groups doc/PANEL.md sets, each with the sentence an operator reads when granting it, whether it is dangerous enough to be worth a second thought, and whether it is the owner's alone. Roles are bundles of these keys rather than a second idea of authority, so a role can be explained by listing what it holds and a grant of one key is the same kind of thing as a role. Nothing here decides anything; it is the table every decision is made against, and a route naming a key this table does not hold is a mistake caught when routes register rather than when somebody is wrongly let in.
 */

#ifndef AMBROSE_PANELPERMISSIONS_H
#define AMBROSE_PANELPERMISSIONS_H

#include "Types.h"

#include <string>
#include <string_view>
#include <vector>

enum class PanelRole : uint8
{
    Owner,
    Admin,
    Operator,
    GameMaster,
    Viewer
};

struct PanelPermission
{
    std::string_view Group;
    std::string_view Key;
    std::string_view Description;
    bool Danger = false;
    bool OwnerOnly = false;
    bool OptIn = false;
};

class PanelPermissions
{
public:
    PanelPermissions() = delete;

    static std::vector<PanelPermission> const& All();
    static PanelPermission const* Find(std::string_view key);
    static bool Holds(std::string_view key) { return Find(key) != nullptr; }

    static std::string_view NameOf(PanelRole role) noexcept;
    static bool ParseRole(std::string_view text, PanelRole& role) noexcept;
    static std::vector<PanelRole> const& Roles();
    static std::vector<std::string_view> const& KeysOf(PanelRole role);
    static bool RoleAllows(PanelRole role, std::string_view key);

    static std::string CatalogJson();
};

#endif
