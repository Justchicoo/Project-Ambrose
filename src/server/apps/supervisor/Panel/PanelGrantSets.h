/*
 * Project Ambrose by Imjustchico
 * One versioned grant per operator and scope: a single row holds the permission keys granted at that scope beside a version that every update matches, so an editor working from yesterday's read gets a 409 instead of silently overwriting today's change, and a grant set is the thing the no-escalation rule plans against; roles stay global and the older one-key rows stay readable, and the decision of what an operator may do reads all three in order.
 */

#ifndef AMBROSE_PANELGRANTSETS_H
#define AMBROSE_PANELGRANTSETS_H

#include "PanelStore.h"
#include "Types.h"

#include <string>
#include <string_view>
#include <vector>

enum class PanelGrantSetSave : uint8
{
    Saved,
    StaleVersion,
    StoreFailed
};

struct PanelGrantSet
{
    int64 UserId = 0;
    std::string Scope = {};
    int64 Version = 0;
    std::vector<std::string> Permissions = {};
    int64 UpdatedEpochMs = 0;
};

class PanelGrantSets
{
public:
    explicit PanelGrantSets(PanelStore& store);

    PanelGrantSets(PanelGrantSets const&) = delete;
    PanelGrantSets& operator=(PanelGrantSets const&) = delete;

    bool Get(int64 userId, std::string_view scope, PanelGrantSet& set, std::string& error) const;
    std::vector<PanelGrantSet> OfUser(int64 userId, std::string& error) const;
    PanelGrantSetSave Save(int64 userId, std::string_view scope, std::vector<std::string> permissions, int64 expectedVersion, int64 byUserId, int64 whenEpochMs, PanelGrantSet& saved, std::string& error);
    bool Holds(int64 userId, std::string_view scope, std::string_view permission, std::string& error) const;
    bool HoldsAny(int64 userId, std::string_view scope, std::string& error) const;
    bool Merge(int64 userId, std::string_view scope, std::vector<std::string> add, int64 byUserId, int64 whenEpochMs, PanelGrantSet& saved, std::string& error);

private:
    static PanelGrantSet Read(int64 userId, PanelStore::Statement const& row);
    static std::string PermissionsJson(std::vector<std::string> const& permissions);

    PanelStore& _store;
};

#endif
