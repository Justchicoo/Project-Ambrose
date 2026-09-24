/*
 * Project Ambrose by Imjustchico
 * Deciding, and the reasoning written where it can be read. The order matters more than any single rule: scope before permission, so that being refused never tells somebody an app exists; role before grant, so that a person's authority can be explained by naming their role; and the recording of a dangerous permission before the answer is returned, so an attempt that failed is still an attempt somebody made.
 */

#include "PanelAuthorization.h"

#include "PanelGrants.h"
#include "PanelUsers.h"

PanelAuthorization::PanelAuthorization(PanelGrants& grants, Finder finder, Recorder recorder)
    : _grants(grants), _finder(std::move(finder)), _recorder(std::move(recorder))
{
}

std::string PanelAuthorization::AppInPath(std::string_view path)
{
    constexpr std::string_view Prefix = "/api/apps/";
    if (!path.starts_with(Prefix))
        return {};
    path.remove_prefix(Prefix.size());
    std::size_t const end = path.find('/');
    return std::string(end == std::string_view::npos ? path : path.substr(0, end));
}

PermissionVerdict PanelAuthorization::Weigh(PanelAsking const& asking, std::string_view permission, bool grantedHere, bool holdsAnythingHere, bool named)
{
    if (PanelPermissions::RoleAllows(asking.Role, permission))
        return PermissionVerdict::Allowed;
    if (grantedHere)
        return PermissionVerdict::Allowed;
    if (named && !holdsAnythingHere)
        return PermissionVerdict::OutOfScope;
    return PermissionVerdict::Forbidden;
}

PermissionVerdict PanelAuthorization::Decide(AdminRequest const& request, std::string_view permission)
{
    if (!PanelPermissions::Holds(permission))
        return PermissionVerdict::Forbidden;

    std::optional<PanelUser> const user = _finder ? _finder(request) : std::nullopt;
    if (!user)
        return PermissionVerdict::Forbidden;

    PanelAsking asking;
    asking.UserId = user->Id;
    asking.Role = user->Role;
    asking.IsOwner = user->IsOwner;

    std::string const app = AppInPath(request.Path);
    bool grantedHere = false;
    bool holdsAnythingHere = false;
    if (!app.empty())
    {
        std::string error;
        grantedHere = _grants.Holds(asking.UserId, app, permission, error);
        holdsAnythingHere = grantedHere || _grants.AppsOf(asking.UserId, error).contains(app);
    }

    PermissionVerdict const verdict = Weigh(asking, permission, grantedHere, holdsAnythingHere, !app.empty());

    PanelPermission const* const held = PanelPermissions::Find(permission);
    if (_recorder && held && held->Danger)
        _recorder(request, permission, app, verdict == PermissionVerdict::Allowed);
    return verdict;
}
