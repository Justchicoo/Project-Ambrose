/*
 * Project Ambrose by Imjustchico
 * The operators who sign in to the panel, kept in the supervisor's own store apart from any game account: names held to the same rules a game account's name is, passwords hashed with Argon2id and never kept any other way, one policy on every path that sets one, and a generation per user that a password or a disable bumps so that user's other sessions stop being believed; a password is checked in the same time and with the same answer whether the user is unknown, disabled or simply wrong, so a caller learns nothing from trying.
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

    PanelUserResult Create(std::string_view username, std::string_view password, bool owner, bool mustChange, int64* id, std::string& error);
    PanelUserResult Authenticate(std::string_view username, std::string_view password, PanelUser& user, std::string& error);
    PanelUserResult SetPassword(int64 id, std::string_view password, bool mustChange, std::string& error);
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
