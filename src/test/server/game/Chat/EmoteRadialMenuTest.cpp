/*
 * Project Ambrose by Imjustchico
 * Checks that a radial menu blob is read as the client's EmotesRadialMenuBehavior::LoadRadialMenuData reads it, in each version, and refused when its magic, version, length or slots are wrong, and that a menu is kept only when every emote slot names an emote item the wizard holds with that item's template.
 */

#include "EmoteRadialMenu.h"

#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
    template<typename T>
    void Put(std::string& blob, T value)
    {
        char bytes[sizeof(T)];
        std::memcpy(bytes, &value, sizeof(T));
        blob.append(bytes, sizeof(T));
    }

    std::string Menu(uint32 version, std::vector<RadialMenuSlot> const& slots, uint32 magic = EmoteRadialMenu::Magic)
    {
        std::string blob;
        Put(blob, magic);
        Put(blob, version);
        if (version >= 2)
            Put(blob, uint32{ 1 });
        Put(blob, static_cast<uint16>(slots.size()));
        for (RadialMenuSlot const& slot : slots)
        {
            Put(blob, slot.Slot);
            Put(blob, slot.EmoteId);
            if (version >= 1)
                Put(blob, slot.TemplateId);
            if (version == 3)
                Put(blob, slot.ChatPhraseId);
        }
        return blob;
    }

    struct Held
    {
        std::map<uint64, uint32> Items;

        bool Check(std::vector<RadialMenuSlot> const& slots, std::string& problem) const
        {
            return EmoteRadialMenu::CheckSlots(slots,
                [this](uint64 itemGuid) -> std::optional<uint32>
                {
                    auto const found = Items.find(itemGuid);
                    return found == Items.end() ? std::nullopt : std::optional<uint32>(found->second);
                },
                [](uint32 templateId) { return templateId == 1457184 || templateId == 1457185; }, problem);
        }
    };
}

TEST(EmoteRadialMenuTest, AnEmptyBlobIsAMenuWithNoSlots)
{
    std::string problem;
    std::optional<std::vector<RadialMenuSlot>> const slots = EmoteRadialMenu::Parse("", problem);
    ASSERT_TRUE(slots) << problem;
    EXPECT_TRUE(slots->empty());
}

TEST(EmoteRadialMenuTest, EachVersionIsReadAsTheClientReadsIt)
{
    std::vector<RadialMenuSlot> const written{ { 0, 281474976710657u, 1457184, 0 }, { 3, 0, 0, 267 } };
    for (uint32 version = 0; version < EmoteRadialMenu::Versions; ++version)
    {
        std::string problem;
        std::optional<std::vector<RadialMenuSlot>> const slots = EmoteRadialMenu::Parse(Menu(version, written), problem);
        ASSERT_TRUE(slots) << "version " << version << ": " << problem;
        ASSERT_EQ(slots->size(), 2u);
        EXPECT_EQ((*slots)[0].EmoteId, 281474976710657u);
        EXPECT_EQ((*slots)[0].TemplateId, version >= 1 ? 1457184u : 0u) << "the template id travels from version 1";
        EXPECT_EQ((*slots)[1].ChatPhraseId, version == 3 ? 267u : 0u) << "the chat phrase travels in version 3";
    }
}

TEST(EmoteRadialMenuTest, AWrongOrDamagedBlobIsRefusedWithTheReason)
{
    std::vector<RadialMenuSlot> const one{ { 0, 281474976710657u, 1457184, 0 } };
    std::string const good = Menu(3, one);
    std::string problem;
    EXPECT_FALSE(EmoteRadialMenu::Parse(Menu(3, one, 0x12345678u), problem));
    EXPECT_NE(problem.find("magic"), std::string::npos) << problem;
    EXPECT_FALSE(EmoteRadialMenu::Parse(Menu(4, one), problem));
    EXPECT_NE(problem.find("version is 4"), std::string::npos) << problem;
    EXPECT_FALSE(EmoteRadialMenu::Parse(good.substr(0, good.size() - 1), problem));
    EXPECT_NE(problem.find("ends inside slot 1"), std::string::npos) << problem;
    EXPECT_FALSE(EmoteRadialMenu::Parse(good + "x", problem));
    EXPECT_NE(problem.find("follow its last slot"), std::string::npos) << problem;
    EXPECT_FALSE(EmoteRadialMenu::Parse(Menu(3, { one[0], one[0] }), problem));
    EXPECT_NE(problem.find("fills slot 0 twice"), std::string::npos) << problem;
}

TEST(EmoteRadialMenuTest, OnlyAnEmoteItemTheWizardHoldsWithItsOwnTemplateMayFillASlot)
{
    Held const held{ { { 281474976710657u, 1457184 }, { 281474976710658u, 4740 } } };
    std::string problem;
    EXPECT_TRUE(held.Check({ { 0, 281474976710657u, 1457184, 0 }, { 1, 0, 0, 267 } }, problem)) << problem;
    EXPECT_FALSE(held.Check({ { 0, 281474976710659u, 1457184, 0 } }, problem));
    EXPECT_NE(problem.find("does not hold"), std::string::npos) << problem;
    EXPECT_FALSE(held.Check({ { 0, 281474976710658u, 4740, 0 } }, problem));
    EXPECT_NE(problem.find("not an emote"), std::string::npos) << problem;
    EXPECT_FALSE(held.Check({ { 0, 281474976710657u, 1457185, 0 } }, problem));
    EXPECT_NE(problem.find("but it is of template 1457184"), std::string::npos) << problem;
}
