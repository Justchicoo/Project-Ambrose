/*
 * Project Ambrose by Imjustchico
 * The no-escalation rule, planned before anything is written: an editor adds or removes only keys they hold at the scope, keys they lack stay on the target untouched, the grant set being edited must hold a strict subset of what the editor holds at that scope so nobody edits at or above their own level, nobody edits their own grants, and an invite carries only a role and grants inside what the inviter holds; a plan names the first key that breaks the rule or the resulting permission list when nothing does.
 */

#ifndef AMBROSE_PANELGRANTPOLICY_H
#define AMBROSE_PANELGRANTPOLICY_H

#include "Types.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class PanelUsers;
class PanelGrants;
class PanelGrantSets;

enum class PanelGrantRefusal : uint8
{
    None,
    SelfEdit,
    NotStrictSubset,
    UnknownKey,
    CannotGrant,
    StoreFailed
};

struct PanelGrantPlan
{
    std::vector<std::string> Result = {};
    std::vector<std::string> Added = {};
    std::vector<std::string> Removed = {};
};

struct PanelGrantCheck
{
    PanelGrantRefusal Refusal = PanelGrantRefusal::None;
    std::string Key = {};
    PanelGrantPlan Plan = {};
};

class PanelGrantPolicy
{
public:
    PanelGrantPolicy() = delete;

    static std::string_view Explain(PanelGrantRefusal refusal) noexcept;

    static bool EffectiveKeys(PanelUsers& users, PanelGrants& grants, PanelGrantSets& sets, int64 userId, std::optional<std::string_view> scope, std::set<std::string>& keys, std::string& error);

    static PanelGrantCheck PlanEdit(int64 callerId, int64 targetId, std::set<std::string> const& callerKeys, std::vector<std::string> const& targetKeys,
        std::vector<std::string> const& add, std::vector<std::string> const& remove);

    static PanelGrantCheck PlanInvite(int64 callerId, std::optional<int64> targetId, std::set<std::string> const& callerKeys, std::set<std::string> const& callerKeysAtScope,
        std::string_view role, std::vector<std::string> const& grants);
};

#endif
