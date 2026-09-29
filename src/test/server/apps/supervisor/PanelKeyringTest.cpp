/*
 * Project Ambrose by Imjustchico
 * Tests the supervisor's keyring: it makes its key once and reads the same key back after a restart, so what it sealed still opens and a keyed hash is the same hash; a secret sealed for one purpose or one operator opens for no other, and two purposes never hash alike; the file names only its owner, the system account and the administrators on Windows, where DPAPI leaves no key in it to read, and is mode 0600 in a 0700 folder elsewhere; and a keyring that is there but damaged, or holds a field of the wrong type, stops the panel with its name rather than throwing or being replaced by a new key that would orphan every sealed secret.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelKeyring.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <aclapi.h>
#else
#include <sys/stat.h>
#endif

namespace
{
    std::string ReadAll(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    bool Load(PanelKeyring& keyring, std::filesystem::path const& file, std::string& error)
    {
        std::vector<std::string> notes;
        std::vector<std::string> warnings;
        return keyring.Load(file, notes, warnings, error);
    }

#ifdef _WIN32
    bool SidIs(PSID sid, WELL_KNOWN_SID_TYPE type)
    {
        alignas(DWORD) BYTE known[SECURITY_MAX_SID_SIZE] = {};
        DWORD size = sizeof(known);
        return ::CreateWellKnownSid(type, nullptr, known, &size) && ::EqualSid(sid, known);
    }

    bool SidIsThisUser(PSID sid)
    {
        HANDLE token = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        DWORD needed = 0;
        ::GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
        std::vector<BYTE> user(needed != 0 ? needed : 1);
        bool const read = ::GetTokenInformation(token, TokenUser, user.data(), static_cast<DWORD>(user.size()), &needed) != FALSE;
        ::CloseHandle(token);
        return read && ::EqualSid(sid, reinterpret_cast<TOKEN_USER const*>(user.data())->User.Sid);
    }
#endif
}

TEST(PanelKeyringTest, MakesItsKeyOnceAndReadsTheSameOneBack)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "data" / "keyring";
    std::vector<uint8> const plain{ 1, 2, 3, 4, 5 };
    std::optional<PanelSealed> sealed;
    std::optional<std::vector<uint8>> hash;
    {
        PanelKeyring keyring;
        std::string error;
        ASSERT_TRUE(Load(keyring, file, error)) << error;
        EXPECT_TRUE(keyring.WasCreated());
        EXPECT_TRUE(keyring.IsOpen());
        EXPECT_EQ(keyring.KeyIds(), (std::vector<int64>{ 1 }));
        EXPECT_EQ(keyring.ActiveId(), 1);
        sealed = keyring.Seal(PanelKeyPurpose::TwoFactorSecret, plain, "panel totp secret for user 1");
        hash = keyring.KeyedHash(PanelKeyPurpose::RecoveryCode, 1, "1:7K2QMXR4TD");
        ASSERT_TRUE(sealed.has_value());
        ASSERT_TRUE(hash.has_value());
        EXPECT_EQ(sealed->KeyId, 1);
    }
    ASSERT_TRUE(std::filesystem::exists(file));

    PanelKeyring again;
    std::string error;
    ASSERT_TRUE(Load(again, file, error)) << error;
    EXPECT_FALSE(again.WasCreated()) << "a keyring that is there is read, never made again";
    EXPECT_EQ(again.Unseal(PanelKeyPurpose::TwoFactorSecret, sealed->KeyId, sealed->Bytes, "panel totp secret for user 1"), std::optional<std::vector<uint8>>(plain));
    EXPECT_EQ(again.KeyedHash(PanelKeyPurpose::RecoveryCode, 1, "1:7K2QMXR4TD"), hash);
    EXPECT_FALSE(again.Unseal(PanelKeyPurpose::TwoFactorSecret, sealed->KeyId, sealed->Bytes, "panel totp secret for user 2").has_value())
        << "a secret sealed for one operator does not open as another's";
    EXPECT_FALSE(again.Unseal(PanelKeyPurpose::TwoFactorSecret, 7, sealed->Bytes, "panel totp secret for user 1").has_value()) << "a key the keyring does not hold opens nothing";

    again.Close();
    EXPECT_FALSE(again.IsOpen());
    EXPECT_FALSE(again.Unseal(PanelKeyPurpose::TwoFactorSecret, sealed->KeyId, sealed->Bytes, "panel totp secret for user 1").has_value()) << "a closed keyring holds no key";
}

TEST(PanelKeyringTest, TwoPurposesNeverShareAKey)
{
    LogTestDirectory directory;
    PanelKeyring keyring;
    std::string error;
    ASSERT_TRUE(Load(keyring, directory.Path() / "keyring", error)) << error;
    EXPECT_NE(PanelKeyring::InfoOf(PanelKeyPurpose::TwoFactorSecret), PanelKeyring::InfoOf(PanelKeyPurpose::RecoveryCode));

    std::vector<uint8> const plain{ 9, 8, 7 };
    std::optional<PanelSealed> const sealed = keyring.Seal(PanelKeyPurpose::TwoFactorSecret, plain, "same");
    ASSERT_TRUE(sealed.has_value());
    EXPECT_FALSE(keyring.Unseal(PanelKeyPurpose::RecoveryCode, sealed->KeyId, sealed->Bytes, "same").has_value());
    EXPECT_TRUE(keyring.Unseal(PanelKeyPurpose::TwoFactorSecret, sealed->KeyId, sealed->Bytes, "same").has_value());
    EXPECT_NE(keyring.KeyedHash(PanelKeyPurpose::TwoFactorSecret, 1, "1:ABCDE"), keyring.KeyedHash(PanelKeyPurpose::RecoveryCode, 1, "1:ABCDE"));
}

TEST(PanelKeyringTest, OnlyItsOwnerTheSystemAndAdministratorsMayReadIt)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "held" / "keyring";
    PanelKeyring keyring;
    std::string error;
    ASSERT_TRUE(Load(keyring, file, error)) << error;
#ifdef _WIN32
    PSID owner = nullptr;
    PACL list = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    std::wstring name = file.wstring();
    ASSERT_EQ(::GetNamedSecurityInfoW(name.data(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &list, nullptr, &descriptor), ERROR_SUCCESS);
    SECURITY_DESCRIPTOR_CONTROL control = 0;
    DWORD revision = 0;
    ASSERT_TRUE(::GetSecurityDescriptorControl(descriptor, &control, &revision));
    EXPECT_NE(static_cast<uint32>(control & SE_DACL_PROTECTED), 0u) << "the list inherits nothing from the folder";
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(static_cast<uint32>(list->AceCount), 3u);
    bool thisUser = false;
    bool localSystem = false;
    bool administrators = false;
    for (DWORD index = 0; index < list->AceCount; ++index)
    {
        void* entry = nullptr;
        ASSERT_TRUE(::GetAce(list, index, &entry));
        ACCESS_ALLOWED_ACE const* const allowed = static_cast<ACCESS_ALLOWED_ACE const*>(entry);
        EXPECT_EQ(static_cast<uint32>(allowed->Header.AceType), static_cast<uint32>(ACCESS_ALLOWED_ACE_TYPE));
        PSID const sid = reinterpret_cast<PSID>(const_cast<DWORD*>(&allowed->SidStart));
        thisUser = thisUser || SidIsThisUser(sid);
        localSystem = localSystem || SidIs(sid, WinLocalSystemSid);
        administrators = administrators || SidIs(sid, WinBuiltinAdministratorsSid);
    }
    ::LocalFree(descriptor);
    EXPECT_TRUE(thisUser) << "the account the supervisor runs as may read it";
    EXPECT_TRUE(localSystem);
    EXPECT_TRUE(administrators);
#else
    struct stat status{};
    ASSERT_EQ(::stat(file.c_str(), &status), 0);
    EXPECT_EQ(status.st_mode & 0777, 0600u);
    struct stat folder{};
    ASSERT_EQ(::stat(file.parent_path().c_str(), &folder), 0);
    EXPECT_EQ(folder.st_mode & 0077, 0u) << "the folder holding it is its owner's alone";
#endif
}

TEST(PanelKeyringTest, OnWindowsTheFileHoldsNoKeyInTheClear)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "keyring";
    PanelKeyring keyring;
    std::string error;
    ASSERT_TRUE(Load(keyring, file, error)) << error;
    std::string const raw = ReadAll(file);
    ASSERT_FALSE(raw.empty());
    nlohmann::json const document = nlohmann::json::parse(raw, nullptr, false);
#ifdef _WIN32
    EXPECT_FALSE(document.is_object()) << "DPAPI wraps the keyring, so the file is not its text";
    EXPECT_EQ(raw.find("secret"), std::string::npos);
    EXPECT_EQ(raw.find("\"keys\""), std::string::npos);
#else
    ASSERT_TRUE(document.is_object()) << "without DPAPI the keyring is its text, kept by mode 0600 alone";
    EXPECT_EQ(document.value("schema", 0), PanelKeyring::Schema);
    EXPECT_EQ(document.value("active", int64{ 0 }), 1);
#endif
}

TEST(PanelKeyringTest, AFieldOfTheWrongTypeIsRefusedRatherThanThrown)
{
#ifdef _WIN32
    GTEST_SKIP() << "DPAPI wraps the keyring on Windows, so a keyring written by hand never reaches the reader";
#else
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "keyring";
    std::string const secret = R"("secret":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")";
    std::vector<std::pair<std::string, bool>> const shapes{
        { R"({"schema":"1","active":1,"keys":[{"id":1,)" + secret + "}]}", false },
        { R"({"schema":1,"active":"1","keys":[{"id":1,)" + secret + "}]}", false },
        { R"({"schema":1,"active":1,"keys":[{"id":1,"created_epoch_ms":"soon",)" + secret + "}]}", true },
    };
    for (auto const& [shape, opens] : shapes)
    {
        {
            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            stream << shape;
        }
        PanelKeyring keyring;
        std::string error;
        bool loaded = !opens;
        EXPECT_NO_THROW(loaded = Load(keyring, file, error)) << shape;
        EXPECT_EQ(loaded, opens) << shape << ": " << error;
        if (!opens)
        {
            EXPECT_NE(error.find("keyring"), std::string::npos) << error;
        }
    }
#endif
}

TEST(PanelKeyringTest, ADamagedKeyringStopsThePanelRatherThanMakingANewKey)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "data" / "keyring";
    std::filesystem::create_directories(file.parent_path());
    std::string const damaged = "this is not a keyring any supervisor wrote";
    {
        std::ofstream stream(file, std::ios::binary);
        stream << damaged;
    }

    PanelKeyring keyring;
    std::string error;
    EXPECT_FALSE(Load(keyring, file, error));
    EXPECT_NE(error.find("keyring"), std::string::npos) << error;

    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    ASSERT_TRUE(config.LoadInitial(directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
    Panel panel(harness.GetLog(), directory.Path() / "data", directory.Path());
    std::string startError;
    EXPECT_FALSE(panel.Start(config, startError));
    EXPECT_NE(startError.find(ConfigMgr::PathToUtf8(file.filename())), std::string::npos) << startError;
    EXPECT_FALSE(panel.IsRunning());
    EXPECT_EQ(ReadAll(file), damaged) << "the damaged keyring is left exactly as it was";
    panel.Stop();
}
