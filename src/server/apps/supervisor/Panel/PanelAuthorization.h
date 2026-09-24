/*
 * Project Ambrose by Imjustchico
 * The one place a request is decided. It answers in the order the panel promises: who is asking, then whether the thing they named is theirs to see at all, then whether they may do this to it, so somebody who holds nothing on an app is told there is no such app rather than that they may not touch it, and a refusal never doubles as a list of what exists. A role answers first and a grant only ever adds, because a rule that can take away is a rule nobody can read off a page. A refused permission the catalog calls dangerous is written down whether or not it was allowed, since an attempt is the thing worth knowing about.
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
    using Recorder = std::function<void(AdminRequest const&, std::string_view permission, std::string_view app, bool allowed)>;

    PanelAuthorization(PanelGrants& grants, Finder finder, Recorder recorder);

    PanelAuthorization(PanelAuthorization const&) = delete;
    PanelAuthorization& operator=(PanelAuthorization const&) = delete;

    PermissionVerdict Decide(AdminRequest const& request, std::string_view permission);

    static std::string AppInPath(std::string_view path);
    static PermissionVerdict Weigh(PanelAsking const& asking, std::string_view permission, bool grantedHere, bool holdsAnythingHere, bool named);

private:
    PanelGrants& _grants;
    Finder _finder;
    Recorder _recorder;
};

#endif
