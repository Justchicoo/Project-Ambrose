/*
 * Project Ambrose by Imjustchico
 * Reads every sigil of the user's own r806919 install through the sigil manager, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it: all 25 templates the manifest lists under Sigils/ load, each one's template id the hash of its name, and CombatSigil8Actor places four monsters and four players in eight circles and carries PvE damage and resist limits that are not zero.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectTemplateMgr.h"
#include "SigilMgr.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class SigilMgrClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            ASSERT_TRUE(sTypeRegistry.LoadFromFile(LogConfig::Utf8Path(*dump)));
            sObjectTemplateMgr.SetInstall(LogConfig::Utf8Path(*client));
            std::vector<std::string> errors;
            ASSERT_TRUE(sObjectTemplateMgr.LoadManifest(errors)) << errors.front();
            s_sigils = std::make_unique<SigilMgr>();
            s_sigils->SetInstall(LogConfig::Utf8Path(*client));
            s_loaded = s_sigils->Load(errors);
            s_errors = errors;
        }

        static void TearDownTestSuite()
        {
            s_sigils.reset();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
        }

        void SetUp() override
        {
            if (!s_sigils)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            ASSERT_TRUE(s_loaded) << (s_errors.empty() ? std::string("the sigils did not load") : s_errors.front());
        }

        static inline std::unique_ptr<SigilMgr> s_sigils;
        static inline bool s_loaded = false;
        static inline std::vector<std::string> s_errors;
    };
}

TEST_F(SigilMgrClientTest, EverySigilLoadsKeyedByTheHashOfItsName)
{
    EXPECT_EQ(s_sigils->GetSigils()->Size(), 25u);
}

TEST_F(SigilMgrClientTest, CombatSigil8ActorPlacesFourMonstersAndFourPlayersWithPvELimits)
{
    SigilInfo const* const sigil = s_sigils->GetSigils()->FindByName("CombatSigil8Actor");
    ASSERT_NE(sigil, nullptr);
    for (std::string const& line : sigil->Describe())
        std::cout << line << "\n";
    EXPECT_EQ(sigil->File, "Sigils/CombatSigil8Actor.xml");
    EXPECT_EQ(sigil->Circles.size(), 8u);
    EXPECT_EQ(sigil->CountCircles("MonsterCircle"), 4u);
    EXPECT_EQ(sigil->CountCircles("PlayerCircle"), 4u);
    EXPECT_TRUE(sigil->Combat);
    EXPECT_NE(sigil->PvE.DamageLimit, 0.0f);
    EXPECT_NE(sigil->PvE.DamageK0, 0.0f);
    EXPECT_NE(sigil->PvE.ResistLimit, 0.0f);
    EXPECT_NE(sigil->PvE.ResistK0, 0.0f);
}
