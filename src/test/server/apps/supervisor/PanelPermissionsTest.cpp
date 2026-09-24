/*
 * Project Ambrose by Imjustchico
 * Tests the catalog every authorisation decision is made against: every key is unique and carries a group and a sentence to read, an owner holds everything while an administrator holds everything not kept for the owner, the quieter roles hold only keys the catalog knows, a role never holds a key reserved to the owner unless it is the owner, a viewer can look and cannot act, and the served catalog carries every key with its danger flag so a grant editor can be built from it alone.
 */

#include "PanelPermissions.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <string_view>

TEST(PanelPermissionsTest, EveryKeyIsUniqueAndSaysWhatItIsFor)
{
    std::set<std::string_view> seen;
    for (PanelPermission const& permission : PanelPermissions::All())
    {
        EXPECT_TRUE(seen.insert(permission.Key).second) << permission.Key << " is in the catalog twice";
        EXPECT_FALSE(permission.Group.empty()) << permission.Key << " belongs to no group";
        EXPECT_FALSE(permission.Description.empty()) << permission.Key << " has nothing to read when granting it";
        EXPECT_NE(permission.Key.find('.'), std::string_view::npos) << permission.Key << " is not group.name";
    }
    EXPECT_GT(seen.size(), 90u) << "the catalog is meant to cover the whole panel";
    EXPECT_TRUE(PanelPermissions::Holds("console.write"));
    EXPECT_FALSE(PanelPermissions::Holds("console.everything"));
}

TEST(PanelPermissionsTest, AnOwnerHoldsEverythingAndAnAdministratorHoldsEverythingElse)
{
    EXPECT_EQ(PanelPermissions::KeysOf(PanelRole::Owner).size(), PanelPermissions::All().size());

    for (PanelPermission const& permission : PanelPermissions::All())
    {
        EXPECT_TRUE(PanelPermissions::RoleAllows(PanelRole::Owner, permission.Key)) << permission.Key;
        if (permission.OwnerOnly || permission.OptIn)
        {
            EXPECT_FALSE(PanelPermissions::RoleAllows(PanelRole::Admin, permission.Key)) << permission.Key << " is not an administrator's to hold";
        }
        else
        {
            EXPECT_TRUE(PanelPermissions::RoleAllows(PanelRole::Admin, permission.Key)) << permission.Key;
        }
    }
}

TEST(PanelPermissionsTest, NoRoleHoldsAKeyTheCatalogDoesNotAndNoneButTheOwnerHoldsTheOwnersOwn)
{
    for (PanelRole const role : PanelPermissions::Roles())
    {
        for (std::string_view const key : PanelPermissions::KeysOf(role))
        {
            PanelPermission const* const permission = PanelPermissions::Find(key);
            ASSERT_NE(permission, nullptr) << PanelPermissions::NameOf(role) << " holds " << key << ", which the catalog does not";
            if (role != PanelRole::Owner)
            {
                EXPECT_FALSE(permission->OwnerOnly) << PanelPermissions::NameOf(role) << " holds " << key << ", which is the owner's alone";
            }
        }
    }
}

TEST(PanelPermissionsTest, AViewerLooksAndDoesNotAct)
{
    for (std::string_view const reading : { "status.read", "console.read", "settings.read", "players.read", "activity.read" })
        EXPECT_TRUE(PanelPermissions::RoleAllows(PanelRole::Viewer, reading)) << reading;
    for (std::string_view const acting : { "console.write", "power.restart", "settings.edit", "accounts.ban", "files.write", "backups.restore" })
        EXPECT_FALSE(PanelPermissions::RoleAllows(PanelRole::Viewer, acting)) << acting << " is not a viewer's to do";

    EXPECT_TRUE(PanelPermissions::RoleAllows(PanelRole::Operator, "console.write"));
    EXPECT_FALSE(PanelPermissions::RoleAllows(PanelRole::Operator, "accounts.delete")) << "running the servers is not owning the accounts";
    EXPECT_TRUE(PanelPermissions::RoleAllows(PanelRole::GameMaster, "players.kick"));
    EXPECT_FALSE(PanelPermissions::RoleAllows(PanelRole::GameMaster, "power.restart")) << "a game master acts on the game, not on the process";
}

TEST(PanelPermissionsTest, TheServedCatalogCarriesEnoughToBuildAGrantEditorFrom)
{
    nlohmann::json const body = nlohmann::json::parse(PanelPermissions::CatalogJson(), nullptr, false);
    ASSERT_TRUE(body.is_object());
    EXPECT_EQ(body["schema"], 1);
    ASSERT_EQ(body["permissions"].size(), PanelPermissions::All().size());
    ASSERT_EQ(body["roles"].size(), PanelPermissions::Roles().size());

    for (nlohmann::json const& entry : body["permissions"])
        for (char const* field : { "group", "key", "description", "danger", "owner_only", "opt_in" })
            EXPECT_TRUE(entry.contains(field)) << field << " missing from " << entry.dump();

    bool foundDanger = false;
    for (nlohmann::json const& entry : body["permissions"])
        if (entry["key"] == "power.kill")
        {
            foundDanger = entry["danger"].get<bool>();
        }
    EXPECT_TRUE(foundDanger) << "a key that can lose character state is marked as one to think about";
}

TEST(PanelPermissionsTest, ARoleIsReadBackFromItsName)
{
    PanelRole role = PanelRole::Viewer;
    EXPECT_TRUE(PanelPermissions::ParseRole("owner", role));
    EXPECT_EQ(role, PanelRole::Owner);
    EXPECT_TRUE(PanelPermissions::ParseRole("game master", role));
    EXPECT_EQ(role, PanelRole::GameMaster);
    EXPECT_FALSE(PanelPermissions::ParseRole("Owner", role)) << "names are matched as they are written";
    EXPECT_FALSE(PanelPermissions::ParseRole("superuser", role));
}
