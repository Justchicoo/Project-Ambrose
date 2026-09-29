/*
 * Project Ambrose by Imjustchico
 * Tests the animation list on a Root.wad the test writes: the key of every record of the Animations table in AnimationData/MasterAnimationList.xml is an animation type an emote may name; a type keyed twice, as r806919's list keys one, is one type; a record without a key or with an empty one, an entry that is not DML table XML or a Root.wad without the entry is refused with the reason; and the reload target animations swaps in a list read afresh, while one that fails keeps the list serving.
 */

#include "AnimationListMgr.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "ReloadMgr.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    std::string List(std::string const& records)
    {
        return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<MasterAnimationList>\n  <_TableList>\n    <RECORD><Name TYPE=\"STR\">Animations</Name></RECORD>\n  </_TableList>\n"
               "  <Animations>\n" + records + "  </Animations>\n</MasterAnimationList>\n";
    }

    std::string Record(std::string const& type)
    {
        return "    <RECORD><AnimType KEY=\"TRUE\" TYPE=\"STR\">" + type + "</AnimType><NumAnims TYPE=\"INT\">1</NumAnims></RECORD>\n";
    }

    class AnimationListMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            _list.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            _list.Clear();
        }

        std::filesystem::path Write(std::string const& name, std::string const& text)
        {
            std::filesystem::path const folder = _directory.Path() / "Data" / "GameData";
            std::filesystem::create_directories(folder);
            std::filesystem::path const path = folder / "Root.wad";
            std::vector<uint8> const bytes = KiwadBuilder(2).Add(name, std::string_view(text), true).Build();
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            return path;
        }

        std::shared_ptr<AnimationList const> ReadWritten(std::vector<std::string>& errors)
        {
            std::string error;
            std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(_directory.Path() / "Data" / "GameData" / "Root.wad", error);
            EXPECT_TRUE(root) << error;
            return root ? AnimationListMgr::Read(*root, errors) : nullptr;
        }

        LogTestDirectory _directory;
        AnimationListMgr _list;
    };
}

TEST_F(AnimationListMgrTest, EveryKeyOfTheAnimationsTableIsATypeAnEmoteMayName)
{
    Write(std::string(AnimationListMgr::Entry), List(Record("Wave") + Record("Chat") + Record("DabDance") + Record("Chat")));
    std::vector<std::string> errors;
    std::shared_ptr<AnimationList const> const list = ReadWritten(errors);
    ASSERT_TRUE(list) << errors.front();
    EXPECT_EQ(list->Size(), 3u) << "a type keyed twice is one type";
    EXPECT_TRUE(list->Contains("Wave"));
    EXPECT_TRUE(list->Contains("Chat"));
    EXPECT_TRUE(list->Contains("DabDance"));
    EXPECT_FALSE(list->Contains("wave")) << "names are matched as the client writes them";
    EXPECT_FALSE(list->Contains("Emote_Wave"));
}

TEST_F(AnimationListMgrTest, AListThatCannotBeReadIsRefusedWithTheReason)
{
    std::vector<std::string> errors;
    Write(std::string(AnimationListMgr::Entry), List(Record("Wave") + Record("")));
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("has an animation type with no name"), std::string::npos) << errors.back();

    errors.clear();
    Write(std::string(AnimationListMgr::Entry), List("    <RECORD><NumAnims TYPE=\"INT\">1</NumAnims></RECORD>\n"));
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("record 0 of AnimationData/MasterAnimationList.xml's Animations table has no AnimType"), std::string::npos) << errors.back();

    errors.clear();
    Write(std::string(AnimationListMgr::Entry), "<MasterAnimationList><_TableList>");
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("is not well-formed XML"), std::string::npos) << errors.back();

    errors.clear();
    Write("Other.xml", List(Record("Wave")));
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("AnimationData/MasterAnimationList.xml cannot be read from"), std::string::npos) << errors.back();
}

TEST_F(AnimationListMgrTest, AReloadSwapsInAFreshListAndOneThatFailsKeepsTheListServing)
{
    _list.RegisterReloadTargets();
    Write(std::string(AnimationListMgr::Entry), List(Record("Wave")));
    ReloadOutcome const first = sReloadMgr.Reload(std::string(AnimationListMgr::Target));
    ASSERT_TRUE(first.Ok) << first.Errors.front();
    EXPECT_TRUE(_list.GetList()->Contains("Wave"));

    Write(std::string(AnimationListMgr::Entry), List(Record("Wave") + Record("Cheer")));
    ASSERT_TRUE(sReloadMgr.Reload(std::string(AnimationListMgr::Target)).Ok);
    EXPECT_TRUE(_list.GetList()->Contains("Cheer")) << "a reload reads the list afresh";

    Write(std::string(AnimationListMgr::Entry), "<MasterAnimationList><Animations>");
    ReloadOutcome const failed = sReloadMgr.Reload(std::string(AnimationListMgr::Target));
    EXPECT_FALSE(failed.Ok);
    EXPECT_FALSE(failed.Errors.empty());
    EXPECT_TRUE(_list.GetList()->Contains("Cheer")) << "the list serving stays";
}
