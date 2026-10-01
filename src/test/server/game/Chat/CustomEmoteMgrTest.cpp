/*
 * Project Ambrose by Imjustchico
 * Checks that the custom-emote catalog maps animation names to defaults and all three purchased ownership ranks, and rejects malformed ownership data.
 */

#include "CustomEmoteMgr.h"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

TEST(CustomEmoteStoreTest, ChecksOwnershipAcrossAllThreeRanks)
{
    std::vector<std::string> errors;
    std::optional<CustomEmoteStore> const store = CustomEmoteStore::Build({
        { "Emote0_Fresh", 0, false },
        { "Emote31_LastFirstRankBit", 31, false },
        { "Emote32_FirstSecondRankBit", 32, false },
        { "Emote63_LastSecondRankBit", 63, false },
        { "Emote64_FirstThirdRankBit", 64, false },
        { "Emote95_LastThirdRankBit", 95, false },
    }, errors);

    ASSERT_TRUE(store.has_value()) << (errors.empty() ? "" : errors.front());
    std::array<uint32, CustomEmoteStore::RankCount> ownership{};
    EXPECT_FALSE(store->OwnsAnimation("Emote0_Fresh", ownership));
    EXPECT_FALSE(store->OwnsAnimation("not-a-custom-emote", ownership));

    ownership = { uint32{ 1 } << 0, 0, 0 };
    EXPECT_TRUE(store->OwnsAnimation("Emote0_Fresh", ownership));
    ownership = { uint32{ 1 } << 31, 0, 0 };
    EXPECT_TRUE(store->OwnsAnimation("Emote31_LastFirstRankBit", ownership));
    ownership = { 0, uint32{ 1 } << 0, 0 };
    EXPECT_TRUE(store->OwnsAnimation("Emote32_FirstSecondRankBit", ownership));
    ownership = { 0, uint32{ 1 } << 31, 0 };
    EXPECT_TRUE(store->OwnsAnimation("Emote63_LastSecondRankBit", ownership));
    ownership = { 0, 0, uint32{ 1 } << 0 };
    EXPECT_TRUE(store->OwnsAnimation("Emote64_FirstThirdRankBit", ownership));
    ownership = { 0, 0, uint32{ 1 } << 31 };
    EXPECT_TRUE(store->OwnsAnimation("Emote95_LastThirdRankBit", ownership));
}

TEST(CustomEmoteStoreTest, AllowsDefaultsWithoutPurchasedBits)
{
    std::vector<std::string> errors;
    std::optional<CustomEmoteStore> const store = CustomEmoteStore::Build({
        { "Emote_Default", 0, true },
    }, errors);

    ASSERT_TRUE(store.has_value()) << (errors.empty() ? "" : errors.front());
    EXPECT_TRUE(store->OwnsAnimation("Emote_Default", {}));
}

TEST(CustomEmoteStoreTest, DoesNotUnlockEmotesWithoutAnOwnershipBit)
{
    std::vector<std::string> errors;
    std::optional<CustomEmoteStore> const store = CustomEmoteStore::Build({
        { "Emote_NoOwnershipBit", -1, false },
    }, errors);

    ASSERT_TRUE(store.has_value()) << (errors.empty() ? "" : errors.front());
    EXPECT_FALSE(store->OwnsAnimation("Emote_NoOwnershipBit", { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu }));
}

TEST(CustomEmoteStoreTest, UnlocksBothAnimationsForOnePurchasedBit)
{
    std::vector<std::string> errors;
    std::optional<CustomEmoteStore> const store = CustomEmoteStore::Build({
        { "Emote_Primary", 10, false },
        { "Emote_Alternate", 10, false },
    }, errors);

    ASSERT_TRUE(store.has_value()) << (errors.empty() ? "" : errors.front());
    std::array<uint32, CustomEmoteStore::RankCount> const ownership{ uint32{ 1 } << 10, 0, 0 };
    EXPECT_TRUE(store->OwnsAnimation("Emote_Primary", ownership));
    EXPECT_TRUE(store->OwnsAnimation("Emote_Alternate", ownership));
}

TEST(CustomEmoteStoreTest, AcceptsAnAnimationWhenAnyTemplateVariantIsOwned)
{
    std::vector<std::string> errors;
    std::optional<CustomEmoteStore> const store = CustomEmoteStore::Build({
        { "Emote_Shared", 4, false },
        { "Emote_Shared", 36, false },
    }, errors);

    ASSERT_TRUE(store.has_value()) << (errors.empty() ? "" : errors.front());
    EXPECT_TRUE(store->OwnsAnimation("Emote_Shared", { 0, uint32{ 1 } << 4, 0 }));
    EXPECT_TRUE(store->OwnsAnimation("Emote_Shared", { uint32{ 1 } << 4, 0, 0 }));
    EXPECT_FALSE(store->OwnsAnimation("Emote_Shared", {}));
}

TEST(CustomEmoteStoreTest, RejectsMalformedAnimationData)
{
    std::vector<std::string> errors;
    EXPECT_FALSE(CustomEmoteStore::Build({ { "", 0, false } }, errors).has_value());
    ASSERT_FALSE(errors.empty());
    EXPECT_FALSE(CustomEmoteStore::Build({ { "Emote_Invalid", -2, false } }, errors).has_value());
    ASSERT_FALSE(errors.empty());
    EXPECT_FALSE(CustomEmoteStore::Build({ { "Emote_Invalid", 96, false } }, errors).has_value());
    ASSERT_FALSE(errors.empty());
    EXPECT_FALSE(CustomEmoteStore::Build({}, errors).has_value());
    ASSERT_FALSE(errors.empty());
}
