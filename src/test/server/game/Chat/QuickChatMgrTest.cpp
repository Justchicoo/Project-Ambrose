/*
 * Project Ambrose by Imjustchico
 * Tests the quick chat phrases on a Root.wad the test writes, its QuickChat.xml a BINd tree of QuickChatEntry folders and phrases in a type dump the test writes: every entry with an id is a phrase found by it with the animation it plays, however deep its folder, and a folder is none; a file holding an id twice, a root of another class, an entry that is not BINd or a Root.wad without QuickChat.xml is refused with the reason; and `.reload quickchat` swaps in phrases read afresh, while one whose QuickChat.xml cannot be read keeps the phrases serving and reports why.
 */

#include "BindFile.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "QuickChatMgr.h"
#include "ReloadMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", bool pointer = false)
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", 7 }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    template<typename... Entries>
    std::vector<PropertyObjectPtr> Children(Entries&&... entries)
    {
        std::vector<PropertyObjectPtr> children;
        (children.push_back(std::forward<Entries>(entries)), ...);
        return children;
    }

    std::string Dump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        Json entry = Json::object();
        entry["m_chatID"] = Property("unsigned int", "m_chatID", 0);
        entry["m_label"] = Property("std::string", "m_label", 1);
        entry["m_text"] = Property("std::string", "m_text", 2);
        entry["m_charAnim"] = Property("std::string", "m_charAnim", 3);
        entry["m_categoryMask"] = Property("unsigned int", "m_categoryMask", 4);
        entry["m_dynFolder"] = Property("int", "m_dynFolder", 5);
        entry["m_supportsMore"] = Property("bool", "m_supportsMore", 6);
        entry["m_membersOnly"] = Property("bool", "m_membersOnly", 7);
        entry["m_unlockKey"] = Property("std::string", "m_unlockKey", 8);
        entry["m_faceAnim"] = Property("std::string", "m_faceAnim", 9);
        entry["m_sound"] = Property("std::string", "m_sound", 10);
        entry["m_childEntries"] = Property("class SharedPointer<class QuickChatEntry>", "m_childEntries", 11, "List", true);
        AddClass(classes, "class QuickChatEntry", Json::array({ "PropertyClass" }), entry);
        Json other = Json::object();
        other["m_chatID"] = Property("unsigned int", "m_chatID", 0);
        AddClass(classes, "class NotQuickChat", Json::array({ "PropertyClass" }), other);
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }

    class QuickChatMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _views.Add(QuickChatEntryView::Definition);
            sReloadMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            ASSERT_TRUE(sTypeRegistry.LoadFromText(Dump(), "quickchat.json")) << sTypeRegistry.GetErrors().front();
            _catalog = sTypeRegistry.GetCatalog();
            sQuickChatMgr.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            sQuickChatMgr.Clear();
            sQuickChatMgr.SetInstall({});
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
        }

        PropertyObjectPtr Entry(uint32 id, std::string const& label, std::string const& animation = {}, std::vector<PropertyObjectPtr> children = {})
        {
            PropertyObjectPtr entry = PropertyObject::Create(_catalog, "class QuickChatEntry");
            EXPECT_TRUE(entry);
            EXPECT_EQ(entry->Set("m_chatID", id), PropertySetResult::Ok);
            EXPECT_EQ(entry->Set("m_label", label), PropertySetResult::Ok);
            EXPECT_EQ(entry->Set("m_charAnim", animation), PropertySetResult::Ok);
            PropertyValue::List list;
            for (PropertyObjectPtr& child : children)
                list.emplace_back(std::move(child));
            EXPECT_EQ(entry->Set("m_childEntries", std::move(list)), PropertySetResult::Ok);
            return entry;
        }

        void Write(std::string const& name, std::vector<uint8> const& bytes)
        {
            std::filesystem::path const folder = _directory.Path() / "Data" / "GameData";
            std::filesystem::create_directories(folder);
            std::vector<uint8> const archive = KiwadBuilder(2).Add(name, bytes, true).Build();
            std::ofstream stream(folder / "Root.wad", std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
        }

        void WritePhrases(PropertyObjectPtr const& root)
        {
            EncodeResult const written = BindFile::Write(root.get());
            ASSERT_TRUE(written.Ok()) << written.Detail;
            Write(std::string(QuickChatMgr::Entry), written.Bytes);
        }

        std::shared_ptr<QuickChatPhrases const> ReadWritten(std::vector<std::string>& errors)
        {
            std::string error;
            std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(_directory.Path() / "Data" / "GameData" / "Root.wad", error);
            EXPECT_TRUE(root) << error;
            return root ? QuickChatMgr::Read(*root, _catalog, errors) : nullptr;
        }

        PropertyObjectPtr Tree()
        {
            return Entry(0, "chatEntryKey0", {}, Children(Entry(0, "chatEntryKey1", {}, Children(Entry(44, "chatEntryKey93", "Wave"))), Entry(45, "chatEntryKey94")));
        }

        TypedViewRegistry _views;
        TypeCatalogPtr _catalog;
        LogTestDirectory _directory;
    };
}

TEST_F(QuickChatMgrTest, EveryEntryWithAnIdIsAPhraseAndAFolderIsNone)
{
    WritePhrases(Tree());
    std::vector<std::string> errors;
    std::shared_ptr<QuickChatPhrases const> const phrases = ReadWritten(errors);
    ASSERT_TRUE(phrases) << errors.front();
    EXPECT_EQ(phrases->Size(), 2u);
    QuickChatPhrase const* const wave = phrases->Find(44);
    ASSERT_NE(wave, nullptr) << "a phrase two folders deep";
    EXPECT_EQ(wave->Label, "chatEntryKey93");
    EXPECT_EQ(wave->CharAnim, "Wave");
    ASSERT_NE(phrases->Find(45), nullptr);
    EXPECT_TRUE(phrases->Find(45)->CharAnim.empty());
    EXPECT_EQ(phrases->Find(0), nullptr) << "a folder is not a phrase";
    EXPECT_EQ(phrases->Find(46), nullptr);
}

TEST_F(QuickChatMgrTest, AFileThatCannotBeReadAsPhrasesIsRefusedWithTheReason)
{
    std::vector<std::string> errors;
    WritePhrases(Entry(0, "root", {}, Children(Entry(44, "one"), Entry(44, "two"))));
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("QuickChat.xml holds the chat id 44 more than once"), std::string::npos) << errors.back();

    errors.clear();
    PropertyObjectPtr const other = PropertyObject::Create(_catalog, "class NotQuickChat");
    WritePhrases(other);
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("holds a class NotQuickChat where a QuickChatEntry belongs"), std::string::npos) << errors.back();

    errors.clear();
    Write(std::string(QuickChatMgr::Entry), std::vector<uint8>{ 'n', 'o', 't', ' ', 'b', 'i', 'n', 'd' });
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("QuickChat.xml does not decode"), std::string::npos) << errors.back();

    errors.clear();
    Write("Other.xml", std::vector<uint8>{ 1, 2, 3 });
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("QuickChat.xml cannot be read from"), std::string::npos) << errors.back();
}

TEST_F(QuickChatMgrTest, AFailedReloadOfQuickChatKeepsTheOldPhrasesAndReportsTheError)
{
    sQuickChatMgr.RegisterReloadTargets();
    WritePhrases(Tree());
    ReloadOutcome const first = sReloadMgr.Reload(QuickChatMgr::Target);
    ASSERT_TRUE(first.Ok) << first.Errors.front();
    ASSERT_NE(sQuickChatMgr.GetPhrases()->Find(44), nullptr);

    WritePhrases(Entry(0, "root", {}, Children(Entry(44, "one"), Entry(47, "added"))));
    ASSERT_TRUE(sReloadMgr.Reload(QuickChatMgr::Target).Ok);
    EXPECT_NE(sQuickChatMgr.GetPhrases()->Find(47), nullptr) << "a reload reads the phrases afresh";
    EXPECT_EQ(sQuickChatMgr.GetPhrases()->Find(45), nullptr);

    Write(std::string(QuickChatMgr::Entry), std::vector<uint8>{ 'b', 'a', 'd' });
    ReloadOutcome const failed = sReloadMgr.Reload(QuickChatMgr::Target);
    EXPECT_FALSE(failed.Ok);
    ASSERT_FALSE(failed.Errors.empty());
    EXPECT_NE(failed.Errors.front().find("QuickChat.xml does not decode"), std::string::npos) << failed.Errors.front();
    EXPECT_NE(sQuickChatMgr.GetPhrases()->Find(47), nullptr) << "the phrases serving stay";
}
