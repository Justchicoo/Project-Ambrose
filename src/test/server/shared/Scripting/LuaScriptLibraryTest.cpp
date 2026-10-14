/*
 * Project Ambrose by Imjustchico
 * Tests the Lua script library on a Root.wad the test writes: every .lua entry of the Scripts folder is kept by its name under that folder and nothing else is, a lookup finds it whatever the case or slashes and with or without the folder, and the reader it hands a state includes from it.
 */

#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "LuaScriptLibrary.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    std::unique_ptr<KiwadArchive> WriteRoot(LogTestDirectory const& directory, KiwadBuilder const& builder)
    {
        std::filesystem::path const path = directory.Path() / "Root.wad";
        std::vector<uint8> const bytes = builder.Build();
        {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        std::string error;
        std::unique_ptr<KiwadArchive> root = KiwadArchive::Open(path, error);
        EXPECT_TRUE(root) << error;
        return root;
    }
}

TEST(LuaScriptLibraryTest, OnlyTheLuaEntriesOfTheScriptsFolderAreKeptByTheirNameUnderIt)
{
    LogTestDirectory directory;
    KiwadBuilder builder(2);
    builder.Add("Scripts/WorldBuilding/CompleteWC.lua", std::string_view("function OpenKrok(args) end"), true)
        .Add("Scripts/class.lua", std::string_view("Class = {}"), false)
        .Add("Scripts/readme.txt", std::string_view("not a script"), false)
        .Add("GUI/Scripts/fake.lua", std::string_view("not under Scripts"), false);
    std::unique_ptr<KiwadArchive> const root = WriteRoot(directory, builder);
    ASSERT_TRUE(root);
    std::vector<std::string> errors;
    std::shared_ptr<LuaScriptLibrary const> const library = LuaScriptLibrary::Read(*root, errors);
    ASSERT_TRUE(library) << errors.front();
    EXPECT_EQ(library->GetNames(), (std::vector<std::string>{ "WorldBuilding/CompleteWC.lua", "class.lua" }));
    EXPECT_EQ(library->Find("Scripts\\worldbuilding\\COMPLETEWC.lua"), "function OpenKrok(args) end");
    EXPECT_EQ(library->Find("WorldBuilding/CompleteWC.lua"), "function OpenKrok(args) end");
    EXPECT_FALSE(library->Find("readme.txt"));
}

TEST(LuaScriptLibraryTest, TheReaderItHandsAStateIncludesFromIt)
{
    std::shared_ptr<LuaScriptLibrary const> const library = LuaScriptLibrary::Build({ { "Scripts/main.lua", "Include('Helper.lua') Done = Helped" }, { "Scripts/helper.lua", "Helped = true" } });
    std::string error;
    std::unique_ptr<LuaState> const state = LuaState::Create(LuaScriptLibrary::ReaderOf(library), nullptr, LuaLimits{}, error);
    ASSERT_TRUE(state) << error;
    EXPECT_TRUE(state->Include("MAIN.lua", error)) << error;
    LuaScriptCensus const census = LuaState::Census(LuaScriptLibrary::ReaderOf(library), "main.lua");
    EXPECT_EQ(census.Files, (std::vector<std::string>{ "main.lua", "Helper.lua" }));
}
