/*
 * Project Ambrose by Imjustchico
 * Tests the Ghidra decompiler's side that needs no Ghidra: the script's markers read back into each function asked for, code and failures alike, found after any prefix Ghidra puts on a line, output from a project holding another build refused and output with no script run reported with Ghidra's own error; a project named by its .gpr file or its path without one; a function kept in the cache returned without starting anything; and a folder that is not a Ghidra install refused by name.
 */

#include "GhidraDecompiler.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

TEST(GhidraDecompilerTest, OutputUnderTheClientsProgramGivesEachFunctionItsCodeOrItsFailure)
{
    std::vector<std::string> const lines{ "INFO  HEADLESS: execution starts (HeadlessAnalyzer)", "AMBROSE-PROGRAM ABC123", "AMBROSE-BEGIN 0x1000 FUN_1000", "", "void FUN_1000(void)",
        "{", "  return;", "}", "", "AMBROSE-END 0x1000", "AMBROSE-FAILED 0x2000 Decompiler process died", "AMBROSE-BEGIN 0x9999 FUN_9999", "int x;", "AMBROSE-END 0x9999" };
    std::string error;
    std::vector<DecompiledFunction> const functions = GhidraDecompiler::ParseOutput(lines, { 0x1000, 0x2000, 0x3000 }, "abc123", error);
    ASSERT_EQ(functions.size(), 3u) << error;
    EXPECT_TRUE(functions[0].Ok());
    EXPECT_EQ(functions[0].Address, 0x1000u);
    EXPECT_EQ(functions[0].Name, "FUN_1000");
    EXPECT_EQ(functions[0].Code, "void FUN_1000(void)\n{\n  return;\n}");
    EXPECT_EQ(functions[1].Error, "Decompiler process died");
    EXPECT_EQ(functions[2].Error, "Ghidra printed nothing for it") << "a function not asked for is ignored, and one asked for but missing is named";
}

TEST(GhidraDecompilerTest, MarkersAreFoundAfterAPrefixGhidraPutsOnTheLine)
{
    std::vector<std::string> const lines{ "AmbroseDecompile.py> AMBROSE-PROGRAM abc", "AmbroseDecompile.py> AMBROSE-BEGIN 0x10 Sample::Load", "void f(void) {}",
        "AmbroseDecompile.py> AMBROSE-END 0x10" };
    std::string error;
    std::vector<DecompiledFunction> const functions = GhidraDecompiler::ParseOutput(lines, { 0x10 }, "ABC", error);
    ASSERT_EQ(functions.size(), 1u) << error;
    EXPECT_EQ(functions[0].Name, "Sample::Load");
    EXPECT_EQ(functions[0].Code, "void f(void) {}");
}

TEST(GhidraDecompilerTest, OutputFromAnotherBuildOrWithoutTheScriptIsRefused)
{
    std::string error;
    EXPECT_TRUE(GhidraDecompiler::ParseOutput({ "AMBROSE-PROGRAM deadbeef", "AMBROSE-BEGIN 0x10 f", "x", "AMBROSE-END 0x10" }, { 0x10 }, "abc", error).empty());
    EXPECT_NE(error.find("another build of the client, SHA-256 deadbeef"), std::string::npos) << error;

    error.clear();
    EXPECT_TRUE(GhidraDecompiler::ParseOutput({ "INFO  HEADLESS: execution starts", "ERROR Could not find project: K:\\Tools\\missing (HeadlessAnalyzer)" }, { 0x10 }, "abc", error).empty());
    EXPECT_NE(error.find("Could not find project"), std::string::npos) << error;
}

TEST(GhidraDecompilerTest, AProjectIsNamedByItsGprFileOrItsPathWithoutOne)
{
    std::optional<GhidraProject> const file = GhidraProject::FromPath("projects/r806919/Client.gpr");
    ASSERT_TRUE(file);
    EXPECT_EQ(file->Location, std::filesystem::path("projects/r806919"));
    EXPECT_EQ(file->Name, "Client");
    EXPECT_EQ(file->File(), std::filesystem::path("projects/r806919") / "Client.gpr");
    std::optional<GhidraProject> const bare = GhidraProject::FromPath("projects/Other");
    ASSERT_TRUE(bare);
    EXPECT_EQ(bare->Name, "Other");
    EXPECT_FALSE(GhidraProject::FromPath(""));
}

TEST(GhidraDecompilerTest, AFunctionInTheCacheIsReturnedWithoutStartingGhidra)
{
    LogTestDirectory directory;
    std::filesystem::path const cache = directory.Path() / "cache";
    std::filesystem::create_directories(cache);
    std::ofstream(cache / "14153a0a0.c", std::ios::binary) << "FUN_14153a0a0\nvoid FUN_14153a0a0(void) {}";

    GhidraSettings settings;
    settings.Install = directory.Path() / "not-ghidra";
    settings.CacheFolder = cache;
    GhidraDecompiler decompiler(settings);
    std::string error;
    std::vector<DecompiledFunction> const cached = decompiler.Decompile({ 0x14153a0a0 }, {}, error);
    ASSERT_EQ(cached.size(), 1u) << error;
    EXPECT_TRUE(cached[0].Ok());
    EXPECT_EQ(cached[0].Name, "FUN_14153a0a0");
    EXPECT_EQ(cached[0].Code, "void FUN_14153a0a0(void) {}");

    EXPECT_TRUE(decompiler.Decompile({ 0x14153a0a0, 0x1415569e0 }, {}, error).empty()) << "a function the cache lacks needs Ghidra";
    EXPECT_NE(error.find("is not a Ghidra install"), std::string::npos) << error;
}

TEST(GhidraDecompilerTest, TheScriptPrintsEveryMarkerTheParserReads)
{
    std::string_view const script = GhidraDecompiler::Script();
    for (std::string_view const marker : { "AMBROSE-PROGRAM", "AMBROSE-BEGIN", "AMBROSE-END", "AMBROSE-FAILED", "getExecutableSHA256", "getScriptArgs" })
        EXPECT_NE(script.find(marker), std::string_view::npos) << marker;
    EXPECT_EQ(GhidraDecompiler::AddressText(0x14153a0a0), "0x14153a0a0");
}
