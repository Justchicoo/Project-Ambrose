/*
 * Project Ambrose by Imjustchico
 * What one operator may do on one app, beside whatever their role allows everywhere. A grant is a single permission on a single app, which is the smallest useful thing to hand somebody, and it only ever adds: a grant cannot take away what a role gives, so reading a decision is reading two questions in order rather than unpicking a sum. An operator holding no role wide enough and no grant on an app is told that app is not there rather than that they may not touch it, because a list of what exists is itself something to know. Nobody gives or takes their own grant, and any change bumps that one operator's session generation and nobody else's, so what they may do is re-read at once while everybody else stays signed in.
 */

#ifndef AMBROSE_PANELGRANTS_H
#define AMBROSE_PANELGRANTS_H

#include "Types.h"

#include <set>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;

struct PanelGrant
{
    int64 Id = 0;
    int64 UserId = 0;
    std::string App;
    std::string Permission;
    int64 GrantedEpochMs = 0;
};

class PanelGrants
{
public:
    explicit PanelGrants(PanelStore& store);

    PanelGrants(PanelGrants const&) = delete;
    PanelGrants& operator=(PanelGrants const&) = delete;

    bool Give(int64 userId, std::string_view app, std::string_view permission, int64 byUserId, int64 whenEpochMs, std::string& error);
    bool Take(int64 userId, std::string_view app, std::string_view permission, int64 byUserId, std::string& error);
    bool TakeAll(int64 userId, std::string& error);
    std::vector<PanelGrant> Of(int64 userId, std::string& error) const;
    bool Holds(int64 userId, std::string_view app, std::string_view permission, std::string& error) const;
    std::set<std::string, std::less<>> AppsOf(int64 userId, std::string& error) const;

private:
    bool Bump(int64 userId, std::string& error);

    PanelStore& _store;
};

#endif
