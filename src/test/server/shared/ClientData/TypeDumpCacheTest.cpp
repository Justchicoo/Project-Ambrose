/*
 * Project Ambrose by Imjustchico
 * Tests the per-revision type dump cache in real temporary folders with a fake install and a fake extractor run: dumps are named only for plain revisions in an absolute data folder, headers parse with their root keys in any order, the client program hashes as a stream, a current dump is reused without running the extractor, a missing or stale one is rebuilt with the expected command naming the install's absolute path and an input that ends with this process, every failure names its cause, including why no exit code was read, a lock file that cannot be opened fails after a grace without running the extractor, a run with no exit code that left a current dump is used, a relative extractor is checked and run as one absolute path from the working directory, an install path that is not valid Unicode on Windows is refused before running, two callers at once run the extractor once, many callers never run it at the same time while the lock file is removed and created again, a held lock is waited on until the dump is current or the lock is released and is never taken over however long its build runs, a stop ends the wait, a lock file no process holds is taken at once whatever it names, even a running process on this machine, the holder writes its process id, time and host name into the lock file, a holder whose lock file was replaced leaves the new holder's file alone, and the default extractor sits beside the executable.
 */

#include "ChildProcess.h"
#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "SHA256.h"
#include "ScopeExit.h"
#include "StringUtil.h"
#include "TypeDumpCache.h"
#include "Types.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    constexpr char const* Revision = "r806919.Wizard_1_610";
    constexpr uint64 MissingProcessId = 2147483647;
    constexpr std::chrono::seconds TestDeadline{ 60 };
    constexpr std::chrono::seconds LeftoverDeadline{ 5 };
    constexpr int ConcurrentCallers = 4;
    constexpr int CallsPerCaller = 25;

    std::string Hex(SHA256::Digest const& digest)
    {
        std::string hex;
        for (uint8 const byte : digest)
            hex += fmt::format("{:02x}", byte);
        return hex;
    }

    std::string DumpText(std::string const& revision, std::string const& sha)
    {
        return fmt::format("{{\n \"version\": 2,\n \"revision\": \"{}\",\n \"executable_sha256\": \"{}\",\n \"extractor\": \"typeextract\",\n \"classes\": {{}}\n}}\n", revision, sha);
    }

    void PutFile(std::filesystem::path const& path, std::string const& content)
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        std::filesystem::path temporary = path;
        temporary += fmt::format(".{}.partial", std::hash<std::thread::id>{}(std::this_thread::get_id()));
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream << content;
        }
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            std::error_code renamed;
            std::filesystem::rename(temporary, path, renamed);
            if (!renamed)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ADD_FAILURE() << "cannot write " << ClientLocator::PathText(path);
    }

    struct Report
    {
        bool Warning;
        std::string Text;
    };

    struct CacheHarness
    {
        LogTestDirectory Directory;
        std::string Program = "MZ\x90 a fake client program";
        std::filesystem::path Root;
        std::filesystem::path Data;
        std::filesystem::path Extractor;
        ClientInstall Install;
        std::string Sha;

        std::mutex ReportsMutex;
        std::vector<Report> Reports;
        std::atomic<int> Runs{ 0 };
        std::mutex SeenMutex;
        std::optional<ChildProcessOptions> Seen;
        std::function<ChildProcessResult(ChildProcessOptions const&)> Behavior;

        CacheHarness()
        {
            Root = std::filesystem::absolute(Directory.Path() / ConfigMgr::PathFromUtf8("Wizard101 Jos\xC3\xA9"));
            Data = std::filesystem::absolute(Directory.Path() / "data");
            Extractor = std::filesystem::absolute(Directory.Path() / "bin" / "typeextract.exe");
            PutFile(Root / "Data" / "GameData" / "Root.wad", "KIWAD");
            PutFile(Root / "Bin" / "revision.dat", std::string(Revision) + "\n");
            PutFile(Root / "Bin" / "WizardGraphicalClient.exe", Program);
            PutFile(Extractor, "not really a program");
            LocalClientSystem const system;
            std::optional<ClientInstall> const inspected = ClientInstall::Inspect(system, Root);
            EXPECT_TRUE(inspected);
            if (inspected)
            {
                Install = *inspected;
            }
            Sha = Hex(SHA256::GetDigestOf(std::string_view(Program)));
            Behavior = [this](ChildProcessOptions const& options) { return WriteCurrent(options); };
        }

        std::filesystem::path DumpPath() const
        {
            return Data / "types" / (std::string(Revision) + ".json");
        }

        std::filesystem::path LockPath() const
        {
            std::filesystem::path lock = DumpPath();
            lock += ".lock";
            return lock;
        }

        static std::filesystem::path OutOf(ChildProcessOptions const& options)
        {
            auto const out = std::find(options.Arguments.begin(), options.Arguments.end(), "--out");
            return out == options.Arguments.end() || out + 1 == options.Arguments.end() ? std::filesystem::path() : ConfigMgr::PathFromUtf8(*(out + 1));
        }

        ChildProcessResult WriteCurrent(ChildProcessOptions const& options)
        {
            options.OnLine("typeextract: extracting", false);
            PutFile(OutOf(options), DumpText(Revision, Sha));
            ChildProcessResult result;
            result.Started = true;
            result.ExitCode = 0;
            return result;
        }

        TypeDumpCacheOptions Options()
        {
            TypeDumpCacheOptions options;
            options.DataFolder = Data;
            options.Extractor = Extractor;
            options.LockPollInterval = std::chrono::milliseconds(5);
            std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + TestDeadline;
            options.ShouldStop = [deadline] { return std::chrono::steady_clock::now() > deadline; };
            options.Run = [this](ChildProcessOptions const& child)
            {
                ++Runs;
                {
                    std::lock_guard<std::mutex> const lock(SeenMutex);
                    Seen = child;
                }
                return Behavior(child);
            };
            options.Report = [this](bool warning, std::string const& text)
            {
                std::lock_guard<std::mutex> const lock(ReportsMutex);
                Reports.push_back({ warning, text });
            };
            return options;
        }

        std::optional<std::filesystem::path> Ensure(TypeDumpCacheOptions const& options, std::string& error)
        {
            return TypeDumpCache::Ensure(Install, options, error);
        }

        bool Reported(bool warning, std::string const& part)
        {
            std::lock_guard<std::mutex> const lock(ReportsMutex);
            return std::any_of(Reports.begin(), Reports.end(), [&](Report const& report) { return report.Warning == warning && report.Text.find(part) != std::string::npos; });
        }

        bool LockExists() const
        {
            std::error_code error;
            return std::filesystem::exists(LockPath(), error);
        }
    };

    ChildProcessResult Finished(std::optional<int> exitCode)
    {
        ChildProcessResult result;
        result.Started = true;
        result.ExitCode = exitCode;
        return result;
    }

    class HeldBuild
    {
    public:
        HeldBuild(CacheHarness& harness, std::function<ChildProcessResult(ChildProcessOptions const&)> afterRelease)
        {
            TypeDumpCacheOptions options = harness.Options();
            options.Run = [this, &harness, afterRelease = std::move(afterRelease)](ChildProcessOptions const& child)
            {
                ++harness.Runs;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _holding = true;
                    _changed.notify_all();
                    _changed.wait_for(lock, TestDeadline, [this] { return _released; });
                }
                return afterRelease(child);
            };
            _thread = std::thread([this, &harness, options] { _result = harness.Ensure(options, _error); });
        }

        ~HeldBuild()
        {
            Release();
            if (_thread.joinable())
                _thread.join();
        }

        HeldBuild(HeldBuild const&) = delete;
        HeldBuild& operator=(HeldBuild const&) = delete;

        bool WaitUntilHolding()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _changed.wait_for(lock, TestDeadline, [this] { return _holding; });
        }

        void Release()
        {
            std::lock_guard<std::mutex> const lock(_mutex);
            _released = true;
            _changed.notify_all();
        }

        std::optional<std::filesystem::path> Finish(std::string& error)
        {
            Release();
            if (_thread.joinable())
                _thread.join();
            error = _error;
            return _result;
        }

    private:
        std::mutex _mutex;
        std::condition_variable _changed;
        bool _holding = false;
        bool _released = false;
        std::optional<std::filesystem::path> _result;
        std::string _error;
        std::thread _thread;
    };
}

TEST(TypeDumpCacheTest, DumpsAreNamedOnlyForPlainRevisionsInAnAbsoluteDataFolder)
{
    LogTestDirectory directory;
    std::filesystem::path const data = std::filesystem::absolute(directory.Path());
    std::optional<std::filesystem::path> const path = TypeDumpCache::PathFor(data, "r806919.Wizard_1_610");
    ASSERT_TRUE(path);
    EXPECT_EQ(ClientLocator::PathText(*path), ClientLocator::PathText(data / "types" / "r806919.Wizard_1_610.json"));
    EXPECT_TRUE(TypeDumpCache::PathFor(data, "r1-beta_2.A"));
    std::vector<std::string> const refused = { "", ".", "..", "r1/x", "r1\\x", "r 1", "r1:x", "r\xC3\xA9", std::string("r1\0x", 4) };
    for (std::string const& revision : refused)
        EXPECT_FALSE(TypeDumpCache::PathFor(data, revision)) << revision;
    EXPECT_FALSE(TypeDumpCache::PathFor("relative/data", "r1"));
    EXPECT_FALSE(TypeDumpCache::PathFor("", "r1"));
    EXPECT_EQ(ClientLocator::DumpHeaderBytes, TypeDumpCache::HeaderBytes);
}

TEST(TypeDumpCacheTest, HeadersParseWithTheirRootKeysInAnyOrder)
{
    LogTestDirectory directory;
    auto const header = [&directory](std::string const& content)
    {
        return TypeDumpCache::ReadHeader(directory.Write("dump.json", content));
    };

    std::optional<TypeDumpHeader> const written = header("{\n \"version\": 2,\n \"revision\": \"r806919.Wizard_1_610\",\n \"executable_sha256\": \"abc123\",\n \"extractor\": \"typeextract\",\n \"classes\": {\"A\": \"" + std::string(20000, 'x') + "\"}\n}\n");
    ASSERT_TRUE(written);
    EXPECT_EQ(written->Revision, "r806919.Wizard_1_610");
    EXPECT_EQ(written->ExecutableSha256, "abc123");
    EXPECT_EQ(written->Extractor, "typeextract");

    std::optional<TypeDumpHeader> const sorted = header("{\"classes\": {\"A\": {\"revision\": \"nested\", \"list\": [1, {\"extractor\": \"deep\"}]}}, \"executable_sha256\": \"ABC\", \"extractor\": \"x\", \"revision\": \"r1\", \"version\": 2}");
    ASSERT_TRUE(sorted);
    EXPECT_EQ(sorted->Revision, "r1");
    EXPECT_EQ(sorted->ExecutableSha256, "ABC");
    EXPECT_EQ(sorted->Extractor, "x");

    std::optional<TypeDumpHeader> const escaped = header("\xEF\xBB\xBF \n{\"revision\": \"r\\u0031\", \"extractor\": \"type\\\"extract\", \"version\": 2}");
    ASSERT_TRUE(escaped);
    EXPECT_EQ(escaped->Revision, "r1");
    EXPECT_EQ(escaped->Extractor, "type\"extract");
    EXPECT_EQ(escaped->ExecutableSha256, "");

    std::optional<TypeDumpHeader> const beyond = header("{\"classes\": {\"A\": \"" + std::string(5000, 'x') + "\"}, \"revision\": \"r1\"}");
    ASSERT_TRUE(beyond);
    EXPECT_EQ(beyond->Revision, "");

    std::optional<TypeDumpHeader> const cut = header("{\"revision\": \"r1\", \"executable_sha256\": \"abc\", \"classes\": {\"A\": \"" + std::string(5000, 'x') + "\"}}");
    ASSERT_TRUE(cut);
    EXPECT_EQ(cut->Revision, "r1");
    EXPECT_EQ(cut->ExecutableSha256, "abc");

    std::optional<TypeDumpHeader> const cutInNumber = header("{\"revision\": \"r1\", \"classes\": [" + std::string(4050, ' ') + "123456789012345678901234567890]}");
    ASSERT_TRUE(cutInNumber);
    EXPECT_EQ(cutInNumber->Revision, "r1");

    std::optional<TypeDumpHeader> const wrongTypes = header("{\"revision\": 5, \"executable_sha256\": null, \"extractor\": [\"x\"], \"version\": 2}");
    ASSERT_TRUE(wrongTypes);
    EXPECT_EQ(wrongTypes->Revision, "");
    EXPECT_EQ(wrongTypes->ExecutableSha256, "");
    EXPECT_EQ(wrongTypes->Extractor, "");

    std::vector<std::string> const refused = { "", "[1]", "\"text\"", "42", "hello", "{\"revision\": \"r1\",", "{\"revision\" \"r1\"}" };
    for (std::string const& bad : refused)
        EXPECT_FALSE(header(bad)) << bad;
    std::optional<TypeDumpHeader> const trailing = header("{\"revision\": \"r1\"} trailing");
    ASSERT_TRUE(trailing);
    EXPECT_EQ(trailing->Revision, "r1");
    EXPECT_FALSE(header("{\"revision\": \"r1\" \"x\": " + std::string(5000, ' ') + "}"));
    EXPECT_FALSE(TypeDumpCache::ReadHeader(directory.Path() / "missing.json"));
    EXPECT_FALSE(TypeDumpCache::ReadHeader(directory.Path()));
}

TEST(TypeDumpCacheTest, TheClientProgramIsHashedAsAStream)
{
    CacheHarness harness;
    std::string program;
    for (int index = 0; program.size() < 2500u * 1024u; ++index)
        program += fmt::format("{:08x}", static_cast<unsigned>(index) * 2654435761u);
    PutFile(harness.Root / "Bin" / "WizardGraphicalClient.exe", program);
    std::string error;
    std::optional<std::string> const sha = TypeDumpCache::ExecutableSha256(harness.Install, error);
    ASSERT_TRUE(sha) << error;
    EXPECT_EQ(*sha, Hex(SHA256::GetDigestOf(std::string_view(program))));

    std::filesystem::remove(harness.Root / "Bin" / "WizardGraphicalClient.exe");
    EXPECT_FALSE(TypeDumpCache::ExecutableSha256(harness.Install, error));
    EXPECT_NE(error.find("there is no Bin/WizardGraphicalClient.exe in the install"), std::string::npos) << error;
    EXPECT_FALSE(TypeDumpCache::IsCurrent(harness.Install, harness.DumpPath(), ""));
}

TEST(TypeDumpCacheTest, ACurrentDumpIsUsedWithoutRunningTheExtractor)
{
    CacheHarness harness;
    PutFile(harness.DumpPath(), DumpText(Revision, harness.Sha));
    std::filesystem::remove(harness.Extractor);
    std::string error;
    std::optional<std::filesystem::path> const dump = harness.Ensure(harness.Options(), error);
    ASSERT_TRUE(dump) << error;
    EXPECT_EQ(ClientLocator::PathText(*dump), ClientLocator::PathText(harness.DumpPath()));
    EXPECT_EQ(harness.Runs.load(), 0);
    EXPECT_TRUE(harness.Reports.empty());
    EXPECT_TRUE(TypeDumpCache::IsCurrent(harness.Install, harness.DumpPath(), harness.Sha));

    PutFile(harness.DumpPath(), DumpText(Revision, Ambrose::ToUpper(harness.Sha)));
    EXPECT_TRUE(harness.Ensure(harness.Options(), error)) << error;
    EXPECT_EQ(harness.Runs.load(), 0);
}

TEST(TypeDumpCacheTest, AMissingOrStaleDumpIsRebuilt)
{
    CacheHarness harness;
    TypeDumpCacheOptions options = harness.Options();
    options.Timeout = std::chrono::seconds(42);
    std::string error;
    std::optional<std::filesystem::path> dump = harness.Ensure(options, error);
    ASSERT_TRUE(dump) << error;
    EXPECT_EQ(ClientLocator::PathText(*dump), ClientLocator::PathText(harness.DumpPath()));
    EXPECT_EQ(harness.Runs.load(), 1);
    ASSERT_TRUE(harness.Seen);
    EXPECT_EQ(ClientLocator::PathText(harness.Seen->Program), ClientLocator::PathText(harness.Extractor));
    EXPECT_EQ(harness.Seen->Arguments, (std::vector<std::string>{ "--client", ConfigMgr::PathToUtf8(std::filesystem::absolute(harness.Install.Root)), "--out", ConfigMgr::PathToUtf8(harness.DumpPath()), "--quiet", "--exit-when-input-ends" }));
    EXPECT_TRUE(harness.Seen->InputEndsWithParent);
    EXPECT_TRUE(ConfigMgr::PathFromUtf8(harness.Seen->Arguments[1]).is_absolute());
    EXPECT_NE(harness.Seen->Arguments[1].find("Jos\xC3\xA9"), std::string::npos);
    EXPECT_EQ(harness.Seen->Timeout, std::chrono::milliseconds(42000));
    EXPECT_FALSE(harness.LockExists());
    EXPECT_TRUE(harness.Reported(false, "Building the type dump for revision r806919.Wizard_1_610"));
    EXPECT_TRUE(harness.Reported(false, "typeextract: extracting"));
    EXPECT_TRUE(harness.Reported(false, "Built the type dump"));

    PutFile(harness.DumpPath(), DumpText("r801440.Wizard_1_610", harness.Sha));
    ASSERT_TRUE(harness.Ensure(options, error)) << error;
    EXPECT_EQ(harness.Runs.load(), 2);
    EXPECT_TRUE(TypeDumpCache::IsCurrent(harness.Install, harness.DumpPath(), harness.Sha));

    PutFile(harness.Root / "Bin" / "WizardGraphicalClient.exe", harness.Program + " patched");
    std::string const patched = Hex(SHA256::GetDigestOf(std::string_view(harness.Program + " patched")));
    EXPECT_FALSE(TypeDumpCache::IsCurrent(harness.Install, harness.DumpPath(), patched));
    harness.Sha = patched;
    ASSERT_TRUE(harness.Ensure(options, error)) << error;
    EXPECT_EQ(harness.Runs.load(), 3);
    EXPECT_TRUE(TypeDumpCache::IsCurrent(harness.Install, harness.DumpPath(), patched));

    PutFile(harness.DumpPath(), "{\"version\": 2, \"classes\": {}}");
    ASSERT_TRUE(harness.Ensure(options, error)) << error;
    EXPECT_EQ(harness.Runs.load(), 4);
}

TEST(TypeDumpCacheTest, FailuresNameTheirCause)
{
    CacheHarness harness;
    std::string error;
    auto const fails = [&harness, &error](TypeDumpCacheOptions const& options, ClientInstall const& install, std::string const& expected)
    {
        error.clear();
        EXPECT_FALSE(TypeDumpCache::Ensure(install, options, error)) << expected;
        EXPECT_NE(error.find(expected), std::string::npos) << error;
        EXPECT_FALSE(harness.LockExists()) << expected;
    };

    TypeDumpCacheOptions options = harness.Options();
    options.DataFolder.clear();
    fails(options, harness.Install, "the Ambrose data folder cannot be found");
    options.DataFolder = "relative/data";
    fails(options, harness.Install, "the Ambrose data folder relative/data is not an absolute path");

    options = harness.Options();
    ClientInstall unnamed = harness.Install;
    unnamed.Revision.clear();
    fails(options, unnamed, "has no readable Bin/revision.dat");
    ClientInstall odd = harness.Install;
    odd.Revision = "r1/../../x";
    fails(options, odd, "names the revision r1/../../x, which cannot name a type dump file");
    ClientInstall elsewhere = harness.Install;
    elsewhere.Root = harness.Directory.Path() / "nowhere";
    fails(options, elsewhere, "there is no Bin/WizardGraphicalClient.exe in the install");

    options.Extractor = harness.Directory.Path() / "bin" / "missing-typeextract";
    fails(options, harness.Install, "needs building, but the type extractor");
    EXPECT_NE(error.find("missing-typeextract was not found"), std::string::npos) << error;
    options.Extractor.clear();
    fails(options, harness.Install, "the type extractor (none named) was not found");
    EXPECT_EQ(harness.Runs.load(), 0);

    options = harness.Options();
    harness.Behavior = [](ChildProcessOptions const&)
    {
        ChildProcessResult result;
        result.Error = "access is denied";
        return result;
    };
    fails(options, harness.Install, "could not be started to build the type dump for revision r806919.Wizard_1_610: access is denied");

    harness.Behavior = [](ChildProcessOptions const& child)
    {
        for (int line = 1; line <= 7; ++line)
            child.OnLine(fmt::format("typeextract: problem {}", line), true);
        child.OnLine("typeextract: progress", false);
        return Finished(3);
    };
    fails(options, harness.Install, "failed to build the type dump for revision r806919.Wizard_1_610 with exit code 3: typeextract: problem 3; typeextract: problem 4; typeextract: problem 5; typeextract: problem 6; typeextract: problem 7");
    EXPECT_EQ(error.find("problem 2"), std::string::npos) << error;
    EXPECT_TRUE(harness.Reported(true, "typeextract: problem 1"));
    EXPECT_TRUE(harness.Reported(false, "typeextract: progress"));

    harness.Behavior = [](ChildProcessOptions const&) { return Finished(1); };
    fails(options, harness.Install, "with exit code 1: it printed no error");

    harness.Behavior = [](ChildProcessOptions const& child)
    {
        child.OnLine("typeextract: out of guest memory", true);
        ChildProcessResult result = Finished(std::nullopt);
        result.Error = "typeextract was ended by signal 9";
        return result;
    };
    fails(options, harness.Install, "failed to build the type dump for revision r806919.Wizard_1_610 with no exit code (typeextract was ended by signal 9): typeextract: out of guest memory");
    EXPECT_EQ(error.find("unknown"), std::string::npos) << error;

    harness.Behavior = [](ChildProcessOptions const&) { return Finished(std::nullopt); };
    fails(options, harness.Install, "with no exit code (its exit code could not be read): it printed no error");

    harness.Behavior = [](ChildProcessOptions const&)
    {
        ChildProcessResult result = Finished(std::nullopt);
        result.TimedOut = true;
        return result;
    };
    fails(options, harness.Install, "did not finish the type dump for revision r806919.Wizard_1_610 within 900 seconds");

    harness.Behavior = [](ChildProcessOptions const& child)
    {
        EXPECT_TRUE(static_cast<bool>(child.ShouldStop));
        ChildProcessResult result = Finished(std::nullopt);
        result.Stopped = true;
        return result;
    };
    options.ShouldStop = [] { return false; };
    fails(options, harness.Install, "building the type dump for revision r806919.Wizard_1_610 was stopped");

    harness.Behavior = [](ChildProcessOptions const&) { return Finished(0); };
    fails(options, harness.Install, "is still not the type dump for revision r806919.Wizard_1_610");
    EXPECT_NE(error.find("it was not written"), std::string::npos) << error;

    harness.Behavior = [&harness](ChildProcessOptions const& child)
    {
        PutFile(CacheHarness::OutOf(child), DumpText("r1", harness.Sha));
        return Finished(0);
    };
    fails(options, harness.Install, "it records the revision r1");

    harness.Behavior = [](ChildProcessOptions const& child)
    {
        PutFile(CacheHarness::OutOf(child), DumpText(Revision, "0000"));
        return Finished(0);
    };
    fails(options, harness.Install, "it records the client program SHA-256 0000 instead of " + harness.Sha);

    harness.Behavior = [](ChildProcessOptions const& child)
    {
        PutFile(CacheHarness::OutOf(child), "garbage");
        return Finished(0);
    };
    fails(options, harness.Install, "it does not start as a JSON object");

    std::filesystem::remove_all(harness.Data / "types");
    PutFile(harness.Data / "types", "a file where the folder should be");
    fails(options, harness.Install, "cannot create the type dump folder");
}

TEST(TypeDumpCacheTest, ALockFileThatCannotBeOpenedFailsNamingItsCauseWithoutRunning)
{
    CacheHarness harness;
    std::filesystem::create_directories(harness.LockPath() / "in the way");
    std::string error;
    std::chrono::steady_clock::time_point const start = std::chrono::steady_clock::now();
    EXPECT_FALSE(harness.Ensure(harness.Options(), error));
    EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::seconds(2));
    EXPECT_NE(error.find("cannot open the lock file " + ClientLocator::PathText(harness.LockPath()) + " to build the type dump for revision r806919.Wizard_1_610: "), std::string::npos) << error;
    EXPECT_EQ(harness.Runs.load(), 0);
    EXPECT_FALSE(harness.Reported(false, "Waiting for another process"));

    std::filesystem::remove_all(harness.LockPath());
    ASSERT_TRUE(harness.Ensure(harness.Options(), error)) << error;
    EXPECT_EQ(harness.Runs.load(), 1);
    EXPECT_FALSE(harness.LockExists());
}

TEST(TypeDumpCacheTest, ARunWithNoExitCodeThatLeftACurrentDumpIsUsed)
{
    CacheHarness harness;
    harness.Behavior = [&harness](ChildProcessOptions const& child)
    {
        ChildProcessResult result = harness.WriteCurrent(child);
        result.ExitCode.reset();
        result.Error = "typeextract could not be waited for: No child processes";
        return result;
    };
    std::string error;
    std::optional<std::filesystem::path> const dump = harness.Ensure(harness.Options(), error);
    ASSERT_TRUE(dump) << error;
    EXPECT_EQ(ClientLocator::PathText(*dump), ClientLocator::PathText(harness.DumpPath()));
    EXPECT_EQ(harness.Runs.load(), 1);
    EXPECT_TRUE(harness.Reported(true, "reported no exit code (typeextract could not be waited for: No child processes), but the type dump"));
    EXPECT_TRUE(harness.Reported(false, "Built the type dump"));
    EXPECT_FALSE(harness.LockExists());
}

TEST(TypeDumpCacheTest, ARelativeExtractorIsCheckedAndRunAsOneAbsolutePathFromTheWorkingDirectory)
{
    CacheHarness harness;
    LogTestDirectory work;
    std::filesystem::path const previous = std::filesystem::current_path();
    std::filesystem::current_path(work.Path());
    ScopeExit const restore([&previous]
    {
        std::error_code ignored;
        std::filesystem::current_path(previous, ignored);
    });
    std::filesystem::path const here = std::filesystem::current_path();
    PutFile(here / "typeextract-here.exe", "not really a program");

    TypeDumpCacheOptions options = harness.Options();
    options.Extractor = "typeextract-here.exe";
    std::string error;
    ASSERT_TRUE(harness.Ensure(options, error)) << error;
    ASSERT_TRUE(harness.Seen);
    EXPECT_TRUE(harness.Seen->Program.is_absolute());
    EXPECT_EQ(ClientLocator::PathText(harness.Seen->Program), ClientLocator::PathText(here / "typeextract-here.exe"));
    EXPECT_TRUE(harness.Reported(false, " with " + ClientLocator::PathText(here / "typeextract-here.exe") + ";"));

    std::filesystem::remove(harness.DumpPath());
    PutFile(here / "tools" / "typeextract-nested.exe", "not really a program");
    options.Extractor = std::filesystem::path("tools") / "typeextract-nested.exe";
    ASSERT_TRUE(harness.Ensure(options, error)) << error;
    EXPECT_EQ(ClientLocator::PathText(harness.Seen->Program), ClientLocator::PathText(here / "tools" / "typeextract-nested.exe"));

    std::filesystem::remove(harness.DumpPath());
    options.Extractor = "typeextract-elsewhere";
    error.clear();
    EXPECT_FALSE(harness.Ensure(options, error));
    EXPECT_NE(error.find("the type extractor " + ClientLocator::PathText(here / "typeextract-elsewhere") + " was not found"), std::string::npos) << error;
    EXPECT_EQ(harness.Runs.load(), 2);

#ifdef _WIN32
    options.Extractor = "typeextract-here";
    ASSERT_TRUE(harness.Ensure(options, error)) << error;
    EXPECT_EQ(ClientLocator::PathText(harness.Seen->Program), ClientLocator::PathText(here / "typeextract-here.exe"));
    EXPECT_EQ(harness.Runs.load(), 3);
#endif
}

TEST(TypeDumpCacheTest, AStopEndsTheWaitBeforeOrWhileBuilding)
{
    CacheHarness harness;
    TypeDumpCacheOptions options = harness.Options();
    options.ShouldStop = [] { return true; };
    std::string error;
    EXPECT_FALSE(harness.Ensure(options, error));
    EXPECT_EQ(error, "building the type dump for revision r806919.Wizard_1_610 was stopped");
    EXPECT_EQ(harness.Runs.load(), 0);

    HeldBuild holder(harness, [](ChildProcessOptions const&) { return Finished(1); });
    ASSERT_TRUE(holder.WaitUntilHolding());
    std::atomic<int> checks{ 0 };
    options.ShouldStop = [&checks] { return ++checks > 5; };
    error.clear();
    EXPECT_FALSE(harness.Ensure(options, error));
    EXPECT_EQ(error, "building the type dump for revision r806919.Wizard_1_610 was stopped");
    EXPECT_EQ(harness.Runs.load(), 1);
    EXPECT_TRUE(harness.LockExists());
    EXPECT_TRUE(harness.Reported(false, "Waiting for another process to build the type dump for revision r806919.Wizard_1_610"));
    std::string holderError;
    EXPECT_FALSE(holder.Finish(holderError));
    EXPECT_NE(holderError.find("with exit code 1"), std::string::npos) << holderError;
    EXPECT_FALSE(harness.LockExists());
}

TEST(TypeDumpCacheTest, TwoCallersAtOnceRunTheExtractorOnce)
{
    CacheHarness harness;
    harness.Behavior = [&harness](ChildProcessOptions const& child)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return harness.WriteCurrent(child);
    };
    TypeDumpCacheOptions const options = harness.Options();
    std::optional<std::filesystem::path> first;
    std::optional<std::filesystem::path> second;
    std::string firstError;
    std::string secondError;
    std::thread other([&] { second = TypeDumpCache::Ensure(harness.Install, options, secondError); });
    first = TypeDumpCache::Ensure(harness.Install, options, firstError);
    other.join();
    ASSERT_TRUE(first) << firstError;
    ASSERT_TRUE(second) << secondError;
    EXPECT_EQ(ClientLocator::PathText(*first), ClientLocator::PathText(harness.DumpPath()));
    EXPECT_EQ(ClientLocator::PathText(*second), ClientLocator::PathText(harness.DumpPath()));
    EXPECT_EQ(harness.Runs.load(), 1);
    EXPECT_FALSE(harness.LockExists());
}

TEST(TypeDumpCacheTest, ManyCallersNeverRunTheExtractorAtOnceWhileTheLockFileComesAndGoes)
{
    CacheHarness harness;
    std::atomic<int> active{ 0 };
    std::atomic<int> most{ 0 };
    harness.Behavior = [&active, &most](ChildProcessOptions const&)
    {
        int const now = ++active;
        int seen = most.load();
        while (now > seen && !most.compare_exchange_weak(seen, now))
        {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        --active;
        return Finished(1);
    };
    TypeDumpCacheOptions options = harness.Options();
    options.LockPollInterval = std::chrono::milliseconds(1);
    std::atomic<int> failed{ 0 };
    std::vector<std::thread> callers;
    for (int caller = 0; caller < ConcurrentCallers; ++caller)
    {
        callers.emplace_back([&harness, &options, &failed]
        {
            for (int call = 0; call < CallsPerCaller; ++call)
            {
                std::string error;
                if (!TypeDumpCache::Ensure(harness.Install, options, error) && error.find("with exit code 1") != std::string::npos)
                    ++failed;
            }
        });
    }
    for (std::thread& caller : callers)
        caller.join();
    EXPECT_EQ(most.load(), 1);
    EXPECT_EQ(harness.Runs.load(), ConcurrentCallers * CallsPerCaller);
    EXPECT_EQ(failed.load(), ConcurrentCallers * CallsPerCaller);
    EXPECT_FALSE(harness.LockExists());
}

TEST(TypeDumpCacheTest, AHeldLockIsWaitedOnUntilTheDumpIsCurrentOrTheLockIsReleased)
{
    CacheHarness harness;
    TypeDumpCacheOptions const options = harness.Options();
    {
        HeldBuild holder(harness, [](ChildProcessOptions const&) { return Finished(0); });
        ASSERT_TRUE(holder.WaitUntilHolding());
        std::thread builder([&harness]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            PutFile(harness.DumpPath(), DumpText(Revision, harness.Sha));
        });
        std::string error;
        std::optional<std::filesystem::path> const dump = harness.Ensure(options, error);
        builder.join();
        ASSERT_TRUE(dump) << error;
        EXPECT_EQ(harness.Runs.load(), 1);
        EXPECT_TRUE(harness.LockExists());
        EXPECT_TRUE(harness.Reported(false, "Waiting for another process"));
        std::string holderError;
        EXPECT_TRUE(holder.Finish(holderError)) << holderError;
        EXPECT_FALSE(harness.LockExists());
    }

    std::filesystem::remove(harness.DumpPath());
    HeldBuild holder(harness, [](ChildProcessOptions const&) { return Finished(1); });
    ASSERT_TRUE(holder.WaitUntilHolding());
    std::thread releaser([&holder]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        holder.Release();
    });
    std::string error;
    std::optional<std::filesystem::path> const dump = harness.Ensure(options, error);
    releaser.join();
    ASSERT_TRUE(dump) << error;
    EXPECT_EQ(harness.Runs.load(), 3);
    std::string holderError;
    EXPECT_FALSE(holder.Finish(holderError));
    EXPECT_NE(holderError.find("with exit code 1"), std::string::npos) << holderError;
    EXPECT_FALSE(harness.LockExists());
}

TEST(TypeDumpCacheTest, AHeldLockIsNeverTakenOverHoweverLongItsBuildRuns)
{
    CacheHarness harness;
    HeldBuild holder(harness, [&harness](ChildProcessOptions const& child) { return harness.WriteCurrent(child); });
    ASSERT_TRUE(holder.WaitUntilHolding());
    std::filesystem::last_write_time(harness.LockPath(), std::filesystem::file_time_type::clock::now() - std::chrono::hours(2));
    TypeDumpCacheOptions options = harness.Options();
    options.Timeout = std::chrono::seconds(1);
    std::atomic<bool> finished{ false };
    std::optional<std::filesystem::path> dump;
    std::string error;
    std::thread waiter([&]
    {
        dump = harness.Ensure(options, error);
        finished = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    EXPECT_FALSE(finished.load());
    EXPECT_EQ(harness.Runs.load(), 1);
    EXPECT_TRUE(harness.LockExists());
    std::string holderError;
    EXPECT_TRUE(holder.Finish(holderError)) << holderError;
    waiter.join();
    ASSERT_TRUE(dump) << error;
    EXPECT_EQ(harness.Runs.load(), 1);
    EXPECT_TRUE(harness.Reported(false, "Waiting for another process"));
    EXPECT_FALSE(harness.Reported(true, "Taking over"));
    EXPECT_FALSE(harness.LockExists());
}

TEST(TypeDumpCacheTest, ALockFileNoProcessHoldsIsTakenAtOnceWhateverItNames)
{
    CacheHarness harness;
    std::string held;
    harness.Behavior = [&harness, &held](ChildProcessOptions const& child)
    {
        std::ifstream stream(harness.LockPath(), std::ios::binary);
        std::getline(stream, held);
        return harness.WriteCurrent(child);
    };
    TypeDumpCacheOptions const options = harness.Options();
    std::string error;
    ASSERT_TRUE(harness.Ensure(options, error)) << error;
    std::size_t const processEnd = held.find(' ');
    ASSERT_NE(processEnd, std::string::npos) << held;
    std::size_t const timeEnd = held.find(' ', processEnd + 1);
    ASSERT_NE(timeEnd, std::string::npos) << held;
    std::string const process = held.substr(0, processEnd);
    std::string const host = held.substr(timeEnd + 1);
    EXPECT_TRUE(Ambrose::StringTo<uint64>(process).value_or(0) > 0) << held;
    EXPECT_TRUE(Ambrose::StringTo<int64>(held.substr(processEnd + 1, timeEnd - processEnd - 1)).value_or(0) > 0) << held;
    ASSERT_FALSE(host.empty()) << held;

    std::vector<std::string> const leftovers = { fmt::format("{} {} {}\n", process, held.substr(processEnd + 1, timeEnd - processEnd - 1), host), fmt::format("{} 0 {}\n", MissingProcessId, host), fmt::format("{} 0 elsewhere-{}\n", process, host), "garbage", "" };
    int runs = harness.Runs.load();
    for (std::string const& leftover : leftovers)
    {
        std::filesystem::remove(harness.DumpPath());
        PutFile(harness.LockPath(), leftover);
        if (leftover.empty())
            std::filesystem::last_write_time(harness.LockPath(), std::filesystem::file_time_type::clock::now() - std::chrono::hours(2));
        {
            std::lock_guard<std::mutex> const lock(harness.ReportsMutex);
            harness.Reports.clear();
        }
        held.clear();
        error.clear();
        TypeDumpCacheOptions quick = options;
        std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + LeftoverDeadline;
        quick.ShouldStop = [deadline] { return std::chrono::steady_clock::now() > deadline; };
        ASSERT_TRUE(harness.Ensure(quick, error)) << leftover << error;
        EXPECT_EQ(harness.Runs.load(), ++runs) << leftover;
        EXPECT_FALSE(harness.Reported(false, "Waiting for another process")) << leftover;
        EXPECT_FALSE(harness.Reported(true, "Taking over")) << leftover;
        EXPECT_TRUE(held.starts_with(process + " ")) << leftover << held;
        EXPECT_TRUE(held.ends_with(" " + host)) << leftover << held;
        EXPECT_FALSE(harness.LockExists()) << leftover;
    }
}

TEST(TypeDumpCacheTest, AHolderNeverRemovesALockFileItNoLongerHolds)
{
    CacheHarness harness;
    HeldBuild first(harness, [](ChildProcessOptions const&) { return Finished(1); });
    ASSERT_TRUE(first.WaitUntilHolding());
    std::filesystem::remove(harness.LockPath());
    HeldBuild second(harness, [](ChildProcessOptions const&) { return Finished(1); });
    ASSERT_TRUE(second.WaitUntilHolding());
    EXPECT_EQ(harness.Runs.load(), 2);
    std::string firstError;
    EXPECT_FALSE(first.Finish(firstError));
    EXPECT_NE(firstError.find("with exit code 1"), std::string::npos) << firstError;
    EXPECT_TRUE(harness.LockExists());

    TypeDumpCacheOptions options = harness.Options();
    std::atomic<int> checks{ 0 };
    options.ShouldStop = [&checks] { return ++checks > 5; };
    std::string error;
    EXPECT_FALSE(harness.Ensure(options, error));
    EXPECT_EQ(error, "building the type dump for revision r806919.Wizard_1_610 was stopped");
    EXPECT_EQ(harness.Runs.load(), 2);

    std::string secondError;
    EXPECT_FALSE(second.Finish(secondError));
    EXPECT_FALSE(harness.LockExists());
}

#ifdef _WIN32
TEST(TypeDumpCacheTest, AnInstallPathThatIsNotValidUnicodeIsRefusedBeforeRunning)
{
    CacheHarness harness;
    ClientInstall install = harness.Install;
    install.Root = harness.Directory.Path() / std::wstring{ L'W', static_cast<wchar_t>(0xD800) };
    PutFile(install.Root / "Bin" / "WizardGraphicalClient.exe", harness.Program);
    std::string error;
    EXPECT_FALSE(TypeDumpCache::Ensure(install, harness.Options(), error));
    EXPECT_NE(error.find("cannot be passed to the type extractor because it is not valid Unicode"), std::string::npos) << error;
    EXPECT_EQ(harness.Runs.load(), 0);
    EXPECT_FALSE(harness.LockExists());
}
#endif

TEST(TypeDumpCacheTest, TheDefaultExtractorSitsBesideTheExecutable)
{
    LogTestDirectory directory;
#ifdef _WIN32
    std::string const native = "typeextract.exe";
    std::string const other = "typeextract";
#else
    std::string const native = "typeextract";
    std::string const other = "typeextract.exe";
#endif
    auto const extractor = [&directory] { return ClientLocator::PathText(TypeDumpCache::DefaultExtractor(directory.Path())); };
    EXPECT_EQ(extractor(), ClientLocator::PathText(directory.Path() / native));
    directory.Write(other, "program");
    EXPECT_EQ(extractor(), ClientLocator::PathText(directory.Path() / other));
    directory.Write(native, "program");
    EXPECT_EQ(extractor(), ClientLocator::PathText(directory.Path() / native));
}
