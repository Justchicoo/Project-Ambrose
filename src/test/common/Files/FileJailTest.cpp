/*
 * Project Ambrose by Imjustchico
 * Tests the jail against real folders: an ordinary path resolves to its entry with its kind, size and identity and a missing one says so; a folder link or junction leaving the root is refused naming where it points, and a symbolic link to a file outside is refused where this user may make one; a link that stays inside is refused unless the root's policy follows it, and then the entry carries the path it really reached; a named pipe, a device and a unix socket are refused before anything opens them for reading or writing, which a writer still blocked on the pipe proves; on Windows a short 8.3 name comes back as its long name, so a rule on the long name still applies; a listing reads the folder's own handle and marks links and special files unopenable; a read is a window of the file, and a file swapped for another after it was resolved is not read; a second hard link shows in the link count; and a rename lands beside or inside another folder without replacing what is there unless asked, a removal takes a file or an empty folder, and both refuse an entry that was swapped after it was resolved.
 */

#include "FileJail.h"
#include "FileLinks.h"
#include "LogTestDirectory.h"
#include "PathRules.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace
{
    std::filesystem::path Canonical(std::filesystem::path const& path)
    {
        std::error_code code;
        std::filesystem::path const canonical = std::filesystem::canonical(path, code);
        return code ? path.lexically_normal() : canonical;
    }

    void WriteFile(std::filesystem::path const& path, std::string const& contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary | std::ios::trunc) << contents;
    }

    Ambrose::JailPath PathOf(std::string_view text)
    {
        Ambrose::JailPathResult const parsed = Ambrose::JailPaths::ParseText(text);
        EXPECT_TRUE(parsed.Ok()) << text << ": " << parsed.Reason;
        return parsed.Path;
    }

    class FileJailTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _root = Canonical(_directory.Path()) / "root";
            _outside = Canonical(_directory.Path()) / "outside";
            std::filesystem::create_directories(_root);
            std::filesystem::create_directories(_outside);
            WriteFile(_outside / "secret.txt", "outside the root");
            std::string error;
            _jail = Ambrose::JailRoot::Open(_root, error);
            ASSERT_NE(_jail, nullptr) << error;
        }

        std::optional<Ambrose::JailEntry> Resolve(std::string_view text, Ambrose::JailError& error, Ambrose::LinkPolicy links = Ambrose::LinkPolicy::Refuse)
        {
            return Ambrose::FileJail::Resolve(*_jail, PathOf(text), links, error);
        }

        LogTestDirectory _directory;
        std::filesystem::path _root;
        std::filesystem::path _outside;
        std::shared_ptr<Ambrose::JailRoot> _jail;
    };
}

TEST_F(FileJailTest, ResolvesOrdinaryPathsAndSaysWhenNothingIsThere)
{
    WriteFile(_root / "logs" / "today.log", "0123456789");
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const file = Resolve("logs/today.log", error);
    ASSERT_TRUE(file.has_value()) << error.Message;
    EXPECT_EQ(file->Stat.Kind, Ambrose::EntryKind::File);
    EXPECT_EQ(file->Stat.Size, 10u);
    EXPECT_TRUE(file->Stat.Identity.Known);
    EXPECT_EQ(file->Relative(), "logs/today.log");
    EXPECT_EQ(file->HostPath, _jail->GetPath() / "logs" / "today.log");

    std::optional<Ambrose::JailEntry> const root = Resolve("", error);
    ASSERT_TRUE(root.has_value()) << error.Message;
    EXPECT_EQ(root->Stat.Kind, Ambrose::EntryKind::Folder);
    EXPECT_TRUE(root->Stat.Identity.Same(_jail->GetIdentity()));

    Ambrose::JailError missing;
    EXPECT_FALSE(Resolve("logs/yesterday.log", missing).has_value());
    EXPECT_EQ(missing.Failure, Ambrose::JailFailure::Missing);
    Ambrose::JailError through;
    EXPECT_FALSE(Resolve("logs/today.log/deeper", through).has_value());
    EXPECT_EQ(through.Failure, Ambrose::JailFailure::Missing) << "a file is not a folder to walk through";
}

TEST_F(FileJailTest, RefusesALinkedFolderLeavingTheRoot)
{
    std::string why;
    ASSERT_TRUE(FileLinks::MakeFolderLink(_root / "escape", _outside, why)) << why;
    for (std::string_view const asked : { "escape", "escape/secret.txt" })
    {
        Ambrose::JailError error;
        EXPECT_FALSE(Resolve(asked, error).has_value()) << asked;
        EXPECT_EQ(error.Failure, Ambrose::JailFailure::Refused) << asked;
        EXPECT_EQ(error.Code, "link") << asked;
        EXPECT_EQ(std::filesystem::path(error.Resolved).lexically_normal(), _outside.lexically_normal()) << asked << " names where the link points";
    }
    Ambrose::JailError followed;
    EXPECT_FALSE(Resolve("escape/secret.txt", followed, Ambrose::LinkPolicy::FollowInside).has_value());
    EXPECT_EQ(followed.Failure, Ambrose::JailFailure::Refused) << "following links that stay inside never follows one that leaves";
    EXPECT_NE(followed.Code, "");
}

TEST_F(FileJailTest, RefusesASymbolicLinkLeavingTheRoot)
{
    std::string why;
    if (!FileLinks::MakeFileLink(_root / "shortcut.txt", _outside / "secret.txt", why))
    {
        GTEST_SKIP() << why;
    }
    Ambrose::JailError error;
    EXPECT_FALSE(Resolve("shortcut.txt", error).has_value());
    EXPECT_EQ(error.Failure, Ambrose::JailFailure::Refused);
    EXPECT_EQ(error.Code, "link");
    EXPECT_EQ(std::filesystem::path(error.Resolved).lexically_normal(), (_outside / "secret.txt").lexically_normal());
    Ambrose::JailError followed;
    EXPECT_FALSE(Resolve("shortcut.txt", followed, Ambrose::LinkPolicy::FollowInside).has_value());
    EXPECT_EQ(followed.Failure, Ambrose::JailFailure::Refused);
}

TEST_F(FileJailTest, RefusesALinkThatStaysInsideUnlessThePolicyFollowsIt)
{
    WriteFile(_root / "inner" / "target.txt", "inside");
    std::string why;
    ASSERT_TRUE(FileLinks::MakeFolderLink(_root / "alias", _root / "inner", why)) << why;

    Ambrose::JailError refused;
    EXPECT_FALSE(Resolve("alias/target.txt", refused).has_value());
    EXPECT_EQ(refused.Code, "link");

    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const followed = Resolve("alias/target.txt", error, Ambrose::LinkPolicy::FollowInside);
    ASSERT_TRUE(followed.has_value()) << error.Message;
    EXPECT_EQ(followed->Stat.Kind, Ambrose::EntryKind::File);
    EXPECT_EQ(followed->Relative(), "inner/target.txt") << "the entry names the path it really reached, which the rules are matched against";
}

TEST_F(FileJailTest, RefusesAFifoADeviceNodeAndASocketBeforeOpeningThem)
{
    std::string why;
    ASSERT_TRUE(FileLinks::MakeSocket(_root / "s", why)) << why;
    Ambrose::JailError bound;
    EXPECT_FALSE(Resolve("s", bound).has_value());
    EXPECT_EQ(bound.Failure, Ambrose::JailFailure::Refused);
    EXPECT_EQ(bound.Code, "socket");

    std::vector<Ambrose::JailListed> listed;
    bool truncated = false;
    Ambrose::JailError listing;
    std::optional<Ambrose::JailEntry> const root = Resolve("", listing);
    ASSERT_TRUE(root.has_value()) << listing.Message;
    ASSERT_TRUE(Ambrose::FileJail::List(*root, 100, listed, truncated, listing)) << listing.Message;
    auto const found = std::find_if(listed.begin(), listed.end(), [](Ambrose::JailListed const& entry) { return entry.Name == "s"; });
    ASSERT_NE(found, listed.end());
    EXPECT_EQ(found->Kind, Ambrose::EntryKind::Socket);
    EXPECT_FALSE(found->Openable);

#ifdef _WIN32
    EXPECT_FALSE(FileLinks::MakeFifo(_root / "pipe", why)) << "an NTFS folder holds no named pipe, and device names are refused by the path parser";
#else
    ASSERT_TRUE(FileLinks::MakeFifo(_root / "pipe", why)) << why;
    std::atomic<bool> opened{ false };
    std::string const fifo = (_root / "pipe").string();
    std::thread writer([&opened, fifo]
    {
        int const handle = ::open(fifo.c_str(), O_WRONLY);
        opened = true;
        if (handle >= 0)
            ::close(handle);
    });
    Ambrose::JailError named;
    std::optional<Ambrose::JailEntry> const reached = Resolve("pipe", named);
    EXPECT_FALSE(reached.has_value());
    EXPECT_EQ(named.Failure, Ambrose::JailFailure::Refused);
    EXPECT_EQ(named.Code, "fifo");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(opened.load()) << "a writer is still waiting for a reader, so the jail never opened the pipe";
    int const release = ::open(fifo.c_str(), O_RDONLY | O_NONBLOCK);
    writer.join();
    if (release >= 0)
    {
        ::close(release);
    }

    std::string error;
    std::shared_ptr<Ambrose::JailRoot> const devices = Ambrose::JailRoot::Open("/dev", error);
    ASSERT_NE(devices, nullptr) << error;
    Ambrose::JailError device;
    EXPECT_FALSE(Ambrose::FileJail::Resolve(*devices, PathOf("null"), Ambrose::LinkPolicy::Refuse, device).has_value());
    EXPECT_EQ(device.Failure, Ambrose::JailFailure::Refused);
    EXPECT_EQ(device.Code, "device");
#endif
}

TEST_F(FileJailTest, ComparesTheFinalPathWithTheRootSoAShortNameCannotSlipPastARule)
{
#ifdef _WIN32
    WriteFile(_root / "ProtectedFolderName" / "held.txt", "held");
    std::wstring const full = (_root / "ProtectedFolderName").wstring();
    std::wstring shortName(MAX_PATH, L'\0');
    DWORD const length = GetShortPathNameW(full.c_str(), shortName.data(), static_cast<DWORD>(shortName.size()));
    if (length == 0 || length >= shortName.size())
    {
        GTEST_SKIP() << "the short name of the test folder could not be read";
    }
    shortName.resize(length);
    std::filesystem::path const alias = std::filesystem::path(shortName).filename();
    if (alias == std::filesystem::path(L"ProtectedFolderName"))
    {
        GTEST_SKIP() << "this volume keeps no 8.3 short names";
    }
    std::string const asked = alias.string() + "/held.txt";
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const entry = Resolve(asked, error);
    ASSERT_TRUE(entry.has_value()) << error.Message;
    EXPECT_EQ(entry->Relative(), "ProtectedFolderName/held.txt");

    Ambrose::PathRules rules;
    std::string problem;
    ASSERT_TRUE(rules.Add("/ProtectedFolderName/", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::BuiltIn, "a protected folder", problem)) << problem;
    EXPECT_FALSE(rules.Match(PathOf(asked).Components, false).has_value()) << "the short name alone does not match the rule";
    EXPECT_TRUE(rules.Match(entry->Components, false).has_value()) << "the name the final path gives does";
#else
    GTEST_SKIP() << "Linux file systems keep no 8.3 short names";
#endif
}

TEST_F(FileJailTest, ListsAFolderFromItsOwnHandleWithRowsItCannotOpen)
{
    WriteFile(_root / "a.txt", "aaa");
    WriteFile(_root / "b" / "c.txt", "c");
    std::string why;
    ASSERT_TRUE(FileLinks::MakeFolderLink(_root / "away", _outside, why)) << why;
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const root = Resolve("", error);
    ASSERT_TRUE(root.has_value()) << error.Message;
    std::vector<Ambrose::JailListed> listed;
    bool truncated = true;
    ASSERT_TRUE(Ambrose::FileJail::List(*root, 100, listed, truncated, error)) << error.Message;
    EXPECT_FALSE(truncated);
    ASSERT_EQ(listed.size(), 3u);
    std::sort(listed.begin(), listed.end(), [](Ambrose::JailListed const& left, Ambrose::JailListed const& right) { return left.Name < right.Name; });
    EXPECT_EQ(listed[0].Name, "a.txt");
    EXPECT_EQ(listed[0].Kind, Ambrose::EntryKind::File);
    EXPECT_EQ(listed[0].Size, 3u);
    EXPECT_TRUE(listed[0].Openable);
    EXPECT_EQ(listed[1].Name, "away");
    EXPECT_EQ(listed[1].Kind, Ambrose::EntryKind::Link);
    EXPECT_FALSE(listed[1].Openable);
    EXPECT_FALSE(listed[1].Problem.empty());
    EXPECT_EQ(listed[2].Name, "b");
    EXPECT_EQ(listed[2].Kind, Ambrose::EntryKind::Folder);

    std::vector<Ambrose::JailListed> first;
    ASSERT_TRUE(Ambrose::FileJail::List(*root, 2, first, truncated, error)) << error.Message;
    EXPECT_TRUE(truncated) << "a folder holding more than the most entries read says so";
    EXPECT_EQ(first.size(), 2u);
}

TEST_F(FileJailTest, ReadsAWindowAndRefusesAFileSwappedAfterItWasResolved)
{
    WriteFile(_root / "notes.txt", "0123456789");
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const entry = Resolve("notes.txt", error);
    ASSERT_TRUE(entry.has_value()) << error.Message;
    std::string bytes;
    ASSERT_TRUE(Ambrose::FileJail::Read(*entry, 2, 5, bytes, error)) << error.Message;
    EXPECT_EQ(bytes, "23456");
    ASSERT_TRUE(Ambrose::FileJail::Read(*entry, 8, 5, bytes, error)) << error.Message;
    EXPECT_EQ(bytes, "89");

    std::optional<Ambrose::JailEntry> const swapped = Resolve("notes.txt", error);
    ASSERT_TRUE(swapped.has_value()) << error.Message;
#ifdef _WIN32
    GTEST_SKIP() << "a read on Windows reopens the very handle the jail resolved, so a name swapped afterwards is never what it reads";
#else
    WriteFile(_root / "replacement.txt", "something else");
    std::error_code code;
    std::filesystem::rename(_root / "replacement.txt", _root / "notes.txt", code);
    ASSERT_FALSE(code) << code.message();
    Ambrose::JailError changed;
    EXPECT_FALSE(Ambrose::FileJail::Read(*swapped, 0, 5, bytes, changed));
    EXPECT_EQ(changed.Code, "changed");
#endif
}

TEST_F(FileJailTest, CountsASecondHardLink)
{
    WriteFile(_root / "one.txt", "one");
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const single = Resolve("one.txt", error);
    ASSERT_TRUE(single.has_value()) << error.Message;
    EXPECT_EQ(single->Stat.Links, 1u);
    std::error_code code;
    std::filesystem::create_hard_link(_root / "one.txt", _root / "two.txt", code);
    ASSERT_FALSE(code) << code.message();
    std::optional<Ambrose::JailEntry> const shared = Resolve("two.txt", error);
    ASSERT_TRUE(shared.has_value()) << error.Message;
    EXPECT_EQ(shared->Stat.Links, 2u);
    EXPECT_TRUE(shared->Stat.Identity.Same(Resolve("one.txt", error)->Stat.Identity)) << "both names are one file";
    Ambrose::FileIdentity identity;
    std::string problem;
    ASSERT_TRUE(Ambrose::FileJail::IdentityOf(_root / "one.txt", identity, problem)) << problem;
    EXPECT_TRUE(identity.Same(shared->Stat.Identity));
}

TEST_F(FileJailTest, RenameMovesAnEntryAndReplacesOnlyWhenAsked)
{
    WriteFile(_root / "a" / "one.txt", "first");
    WriteFile(_root / "b" / "two.txt", "second");
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const source = Resolve("a/one.txt", error);
    std::optional<Ambrose::JailEntry> const target = Resolve("b", error);
    ASSERT_TRUE(source.has_value() && target.has_value()) << error.Message;
    EXPECT_FALSE(Ambrose::FileJail::Rename(*source, *target, "two.txt", false, error));
    EXPECT_EQ(error.Failure, Ambrose::JailFailure::Conflict);
    EXPECT_EQ(error.Code, "exists");
    EXPECT_TRUE(std::filesystem::exists(_root / "a" / "one.txt"));

    Ambrose::JailError moved;
    EXPECT_TRUE(Ambrose::FileJail::Rename(*source, *target, "three.txt", false, moved)) << moved.Message << ": " << moved.Resolved;
    EXPECT_FALSE(std::filesystem::exists(_root / "a" / "one.txt"));
    EXPECT_TRUE(std::filesystem::exists(_root / "b" / "three.txt"));

    Ambrose::JailError again;
    std::optional<Ambrose::JailEntry> const next = Resolve("b/three.txt", again);
    ASSERT_TRUE(next.has_value()) << again.Message;
    EXPECT_TRUE(Ambrose::FileJail::Rename(*next, *target, "two.txt", true, again)) << again.Message;
    std::ifstream stream(_root / "b" / "two.txt", std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(contents, "first");
}

TEST_F(FileJailTest, RenameRefusesAnEntrySwappedAfterItWasResolved)
{
    WriteFile(_root / "swap.txt", "original");
    WriteFile(_root / "keep.txt", "other");
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const source = Resolve("swap.txt", error);
    std::optional<Ambrose::JailEntry> const target = Resolve("", error);
    ASSERT_TRUE(source.has_value() && target.has_value()) << error.Message;
    std::filesystem::remove(_root / "swap.txt");
    std::filesystem::rename(_root / "keep.txt", _root / "swap.txt");
    Ambrose::JailError failure;
    EXPECT_FALSE(Ambrose::FileJail::Rename(*source, *target, "moved.txt", false, failure));
    EXPECT_EQ(failure.Code, "changed");
    EXPECT_TRUE(std::filesystem::exists(_root / "swap.txt"));
    EXPECT_FALSE(std::filesystem::exists(_root / "moved.txt"));
}

TEST_F(FileJailTest, RemoveTakesAFileOrAnEmptyFolderAndNothingElse)
{
    WriteFile(_root / "full" / "inside.txt", "x");
    std::filesystem::create_directories(_root / "empty");
    Ambrose::JailError error;
    std::optional<Ambrose::JailEntry> const full = Resolve("full", error);
    std::optional<Ambrose::JailEntry> const empty = Resolve("empty", error);
    std::optional<Ambrose::JailEntry> const file = Resolve("full/inside.txt", error);
    ASSERT_TRUE(full.has_value() && empty.has_value() && file.has_value()) << error.Message;
    Ambrose::JailError refused;
    EXPECT_FALSE(Ambrose::FileJail::Remove(*full, refused));
    EXPECT_EQ(refused.Code, "not_empty");
    EXPECT_TRUE(Ambrose::FileJail::Remove(*empty, refused)) << refused.Message;
    EXPECT_TRUE(Ambrose::FileJail::Remove(*file, refused)) << refused.Message;
    EXPECT_FALSE(std::filesystem::exists(_root / "empty"));
    EXPECT_FALSE(std::filesystem::exists(_root / "full" / "inside.txt"));
}
