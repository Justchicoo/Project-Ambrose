/*
 * Project Ambrose by Imjustchico
 * Decides panel access in scope-before-permission order, so a caller learns nothing about an app they cannot see, and reports dangerous permission verdicts for the audit log.
 */

#ifndef AMBROSE_PANELAUTHORIZATION_H
#define AMBROSE_PANELAUTHORIZATION_H

#include "AdminRouter.h"
#include "PanelPermissions.h"
#include "Types.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

struct PanelUser;
class PanelGrants;

struct PanelAsking
{
    int64 UserId = 0;
    PanelRole Role = PanelRole::Viewer;
    bool IsOwner = false;
};

class PanelAuthorization
{
public:
    using Finder = std::function<std::optional<PanelUser>(AdminRequest const&)>;
    using Recorder = std::function<void(AdminRequest const&, std::string_view permission, std::string_view app, PermissionVerdict verdict)>;

    PanelAuthorization(PanelGrants& grants, Finder finder, Recorder recorder);

    PanelAuthorization(PanelAuthorization const&) = delete;
    PanelAuthorization& operator=(PanelAuthorization const&) = delete;

    PermissionVerdict Decide(AdminRequest const& request, std::string_view permission);
    uint8 CommandLevel(AdminRequest const& request);

    static std::string AppInPath(std::string_view path);
    static PermissionVerdict Weigh(PanelAsking const& asking, std::string_view permission, bool grantedHere, bool holdsAnythingHere, bool named);

private:
    PanelGrants& _grants;
    Finder _finder;
    Recorder _recorder;
};

#endif
