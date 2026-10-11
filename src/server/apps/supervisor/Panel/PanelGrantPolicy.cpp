/*
 * Project Ambrose by Imjustchico
 * Plans a grant edit or an invite against what the caller holds: unknown keys are refused before anything else, an add names keys the caller lacks one by one, a remove silently keeps the keys the caller lacks, and the set under edit must sit strictly below the caller's own, which is what keeps an operator from touching an administrator's grants or their own; the effective keys read a role, the older one-key rows and the versioned sets together, so nothing granted yesterday escapes today's check.
 */

#include "PanelGrantPolicy.h"
#include "PanelGrants.h"
#include "PanelGrantSets.h"
#include "PanelPermissions.h"
#include "PanelUsers.h"

#include <algorithm>

namespace
{
    bool StrictSubsetOf(std::vector<std::string> const& target, std::set<std::string> const& caller)
    {
        std::set<std::string> const ordered(target.begin(), target.end());
        if (ordered.size() >= caller.size())
            return false;
        return std::includes(caller.begin(), caller.end(), ordered.begin(), ordered.end());
    }
}

std::string_view PanelGrantPolicy::Explain(PanelGrantRefusal refusal) noexcept
{
    switch (refusal)
    {
        case PanelGrantRefusal::None: return "allowed";
        case PanelGrantRefusal::SelfEdit: return "nobody edits their own grants";
        case PanelGrantRefusal::NotStrictSubset: return "the grants under edit are not below your own at this scope";
        case PanelGrantRefusal::UnknownKey: return "a permission the catalog does not hold was named";
        case PanelGrantRefusal::CannotGrant: return "a permission you do not hold was named";
        case PanelGrantRefusal::StoreFailed: return "the panel store could not be read";
    }
    return "refused";
}

bool PanelGrantPolicy::EffectiveKeys(PanelUsers& users, PanelGrants& grants, PanelGrantSets& sets, int64 userId, std::optional<std::string_view> scope, std::set<std::string>& keys, std::string& error)
{
    error.clear();
    keys.clear();
    std::optional<PanelUser> const user = users.FindById(userId, error);
    if (!error.empty() || !user)
    {
        if (error.empty())
            error = "the operator no longer exists";
        return false;
    }
    for (std::string_view const key : PanelPermissions::KeysOf(user->Role))
        keys.emplace(key);
    if (scope)
    {
        std::vector<PanelGrant> const held = grants.Of(userId, error);
        if (!error.empty())
            return false;
        for (PanelGrant const& grant : held)
            if (grant.App == *scope)
                keys.emplace(grant.Permission);
        PanelGrantSet set;
        if (!sets.Get(userId, *scope, set, error) || !error.empty())
            return false;
        for (std::string const& key : set.Permissions)
            keys.emplace(key);
        return true;
    }
    std::vector<PanelGrant> const held = grants.Of(userId, error);
    if (!error.empty())
        return false;
    for (PanelGrant const& grant : held)
        keys.emplace(grant.Permission);
    std::vector<PanelGrantSet> const owned = sets.OfUser(userId, error);
    if (!error.empty())
        return false;
    for (PanelGrantSet const& set : owned)
        for (std::string const& key : set.Permissions)
            keys.emplace(key);
    return true;
}

PanelGrantCheck PanelGrantPolicy::PlanEdit(int64 callerId, int64 targetId, std::set<std::string> const& callerKeys, std::vector<std::string> const& targetKeys,
    std::vector<std::string> const& add, std::vector<std::string> const& remove)
{
    PanelGrantCheck check;
    if (callerId == targetId)
    {
        check.Refusal = PanelGrantRefusal::SelfEdit;
        return check;
    }
    for (std::string const& key : add)
    {
        if (!PanelPermissions::Holds(key))
        {
            check.Refusal = PanelGrantRefusal::UnknownKey;
            check.Key = key;
            return check;
        }
        if (!callerKeys.contains(key))
        {
            check.Refusal = PanelGrantRefusal::CannotGrant;
            check.Key = key;
            return check;
        }
    }
    for (std::string const& key : remove)
        if (!PanelPermissions::Holds(key))
        {
            check.Refusal = PanelGrantRefusal::UnknownKey;
            check.Key = key;
            return check;
        }
    if (!StrictSubsetOf(targetKeys, callerKeys))
    {
        check.Refusal = PanelGrantRefusal::NotStrictSubset;
        return check;
    }
    std::set<std::string> result(targetKeys.begin(), targetKeys.end());
    for (std::string const& key : add)
        if (result.insert(key).second)
            check.Plan.Added.push_back(key);
    for (std::string const& key : remove)
        if (callerKeys.contains(key) && result.erase(key) > 0)
            check.Plan.Removed.push_back(key);
    check.Plan.Result.assign(result.begin(), result.end());
    return check;
}

PanelGrantCheck PanelGrantPolicy::PlanInvite(int64 callerId, std::optional<int64> targetId, std::set<std::string> const& callerKeys, std::set<std::string> const& callerKeysAtScope,
    std::string_view role, std::vector<std::string> const& grants)
{
    PanelGrantCheck check;
    if (targetId && *targetId == callerId)
    {
        check.Refusal = PanelGrantRefusal::SelfEdit;
        return check;
    }
    if (!role.empty())
    {
        PanelRole parsed = PanelRole::Viewer;
        if (!PanelPermissions::ParseRole(role, parsed))
        {
            check.Refusal = PanelGrantRefusal::UnknownKey;
            check.Key = std::string(role);
            return check;
        }
        for (std::string_view const key : PanelPermissions::KeysOf(parsed))
            if (!callerKeys.contains(std::string(key)))
            {
                check.Refusal = PanelGrantRefusal::CannotGrant;
                check.Key = std::string(key);
                return check;
            }
    }
    for (std::string const& key : grants)
    {
        if (!PanelPermissions::Holds(key))
        {
            check.Refusal = PanelGrantRefusal::UnknownKey;
            check.Key = key;
            return check;
        }
        if (!callerKeysAtScope.contains(key))
        {
            check.Refusal = PanelGrantRefusal::CannotGrant;
            check.Key = key;
            return check;
        }
    }
    std::set<std::string> result(grants.begin(), grants.end());
    check.Plan.Result.assign(result.begin(), result.end());
    return check;
}
