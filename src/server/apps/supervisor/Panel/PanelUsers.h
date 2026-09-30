/*
 * Project Ambrose by Imjustchico
 * The operators who sign in to the panel, kept in the supervisor's own store apart from any game account: names held to the same rules a game account's name is, passwords hashed with Argon2id and never kept any other way, one policy on every path that sets one, whether two-factor sign-in is on for them, and a generation per user that a password, a disable or a two-factor change bumps so that user's other sessions stop being believed; a password is checked in the same time and with the same answer whether the user is unknown, disabled or simply wrong, so a caller learns nothing from trying, and a signed-in operator's password is checked again the same way before a change to how they sign in. Making an operator and setting a password each split into a half that checks and hashes and a half that writes, so the slow hash never runs inside an open transaction, and the owner made first who is not disabled can be found for a link that names nobody.
 */

#ifndef AMBROSE_PANELUSERS_H
#define AMBROSE_PANELUSERS_H

#include "AccountText.h"
#include "PanelStore.h"
#include "PanelPermissions.h"
#include "Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class PanelUserResult : uint8
{
    Ok,
    NameTooShort,
    NameTooLong,
    NameInvalid,
    NameTaken,
    PasswordTooShort,
    PasswordTooLong,
    PasswordInvalid,
    PasswordIsTheName,
    UnknownUser,
    Disabled,
    LastOwner,
    Themselves,
    WrongPassword,
    HashFailed,
    StoreFailed
};

struct PanelUser
{
    int64 Id = 0;
    std::string Username;
    std::string DisplayName;
    std::string Email;
    int64 Generation = 1;
    bool Disabled = false;
    bool MustChange = false;
    bool IsOwner = false;
    PanelRole Role = PanelRole::Viewer;
    int64 CreatedEpochMs = 0;
    int64 PasswordSetEpochMs = 0;
    std::optional<int64> SignedInEpochMs;
    bool TwoFactor = false;
};

struct PanelUserDraft
{
    std::string Username = {};
    std::string Hash = {};
    bool Owner = false;
    bool MustChange = false;
};

struct PanelPasswordPolicy
{
    static constexpr uint32 FloorLength = 8;
    static constexpr uint32 DefaultLength = 12;

    uint32 MinLength = DefaultLength;

    PanelUserResult Check(std::string_view username, std::string_view password) const;
};

class PanelUsers
{
public:
    explicit PanelUsers(PanelStore& store);

    PanelUsers(PanelUsers const&) = delete;
    PanelUsers& operator=(PanelUsers const&) = delete;

    void SetPolicy(PanelPasswordPolicy policy);
    PanelPasswordPolicy const& GetPolicy() const { return _policy; }

    static std::string Fold(std::string_view username);
    static std::string_view Explain(PanelUserResult result) noexcept;
    static std::string Unguessable();
    static bool HashPassword(std::string_view password, std::string& hash, std::string& error);
    static bool PasswordMatches(std::string const& hash, std::string_view password);

    bool IsEmpty(std::string& error);
    std::optional<PanelUser> Find(std::string_view username, std::string& error);
    std::optional<PanelUser> FindById(int64 id, std::string& error);
    std::vector<PanelUser> List(std::string& error);
    std::optional<PanelUser> FirstOwner(std::string& error);

    PanelUserResult Create(std::string_view username, std::string_view password, bool owner, bool mustChange, int64* id, std::string& error);
    PanelUserResult PrepareUser(std::string_view username, std::string_view password, bool owner, bool mustChange, PanelUserDraft& draft, std::string& error);
    PanelUserResult InsertUser(PanelUserDraft const& draft, int64* id, std::string& error);
    PanelUserResult Authenticate(std::string_view username, std::string_view password, PanelUser& user, std::string& error);
    PanelUserResult CheckPassword(int64 id, std::string_view password, std::string& error);
    std::optional<int64> BumpGeneration(int64 id, std::string& error);
    PanelUserResult SetPassword(int64 id, std::string_view password, bool mustChange, std::string& error);
    PanelUserResult PreparePassword(int64 id, std::string_view password, std::string& hash, std::string& error);
    PanelUserResult StoreHash(int64 id, std::string const& hash, bool mustChange, std::string& error);
    bool SetDisabled(int64 id, bool disabled, std::string& error);
    PanelUserResult SetRole(int64 id, PanelRole role, int64 byUserId, std::string& error);
    PanelUserResult Remove(int64 id, std::string& error);
    bool IsLastOwner(int64 id, std::string& error);
    uint32 CountOwners(std::string& error);
    bool RecordSignIn(int64 id, std::string& error);

private:
    static PanelUser Read(PanelStore::Statement const& row);

    PanelStore& _store;
    PanelPasswordPolicy _policy;
};

#endif
