/*
 * Project Ambrose by Imjustchico
 * Extracts every template of the user's own r806919 install, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it: all 137423 manifest entries become rows, the 104869 in ObjectData among them, with none left unread; the Ravenwood student 38232 carries its object name, display key, portrait and both its NPC and questing behaviors, the questing one named from its bytes though no class describes it; 39088 is the Golem Tower registrar; the hat 1652259 is a WizItemTemplate under its display key; and building the script twice writes the same rows.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "TemplateExtractor.h"
#include "TemplateScript.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

namespace
{
    class TemplateExtractorClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            std::string error;
            std::optional<TemplateExtraction> extraction = TemplateExtractor::ExtractFromInstall(LogConfig::Utf8Path(*client), LogConfig::Utf8Path(*dump), error);
            ASSERT_TRUE(extraction) << error;
            s_extraction = std::make_unique<TemplateExtraction>(std::move(*extraction));
        }

        static void TearDownTestSuite()
        {
            s_extraction.reset();
        }

        void SetUp() override
        {
            if (!s_extraction)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            ASSERT_TRUE(s_extraction->Ok()) << s_extraction->Errors.front();
        }

        static bool HasBehavior(ExtractedTemplate const& found, std::string const& name)
        {
            return std::any_of(found.Behaviors.begin(), found.Behaviors.end(), [&name](ExtractedBehavior const& behavior) { return behavior.Name == name; });
        }

        static inline std::unique_ptr<TemplateExtraction> s_extraction;
    };
}

TEST_F(TemplateExtractorClientTest, EveryManifestEntryAndEveryObjectDataEntryIsRead)
{
    EXPECT_EQ(s_extraction->ManifestEntries, 137423u);
    EXPECT_EQ(s_extraction->Templates.size(), 137423u);
    EXPECT_EQ(s_extraction->UnreadCount, 0u) << (s_extraction->Unread.empty() ? std::string() : s_extraction->Unread.front());
    EXPECT_EQ(s_extraction->ObjectDataEntries, 104869u);
    EXPECT_EQ(s_extraction->ObjectDataRead, 104869u);
    EXPECT_GT(s_extraction->GetNpcTemplateCount(), 10000u);
}

TEST_F(TemplateExtractorClientTest, TheRavenwoodStudentTheRegistrarAndTheHatAreRows)
{
    ExtractedTemplate const* const student = s_extraction->Find(38232);
    ASSERT_NE(student, nullptr);
    EXPECT_EQ(student->ObjectName, "WC-RAV-NPC06");
    EXPECT_EQ(student->DisplayKey, "WC-NPCs_00000125");
    EXPECT_EQ(student->Icon, "GUI/NpcPortraits/Art_Portrait_Boy_Fire.dds");
    EXPECT_TRUE(HasBehavior(*student, "NPCBehavior"));
    EXPECT_TRUE(HasBehavior(*student, "WizardQuestingBehavior")) << "named from its bytes, though no class describes WizardQuestingBehaviorTemplate";
    EXPECT_TRUE(HasBehavior(*student, "BasicNPCServiceBehavior"));

    ExtractedTemplate const* const registrar = s_extraction->Find(39088);
    ASSERT_NE(registrar, nullptr);
    EXPECT_EQ(registrar->ObjectName, "WC-GTW-Registrar");

    ExtractedTemplate const* const hat = s_extraction->Find(1652259);
    ASSERT_NE(hat, nullptr);
    EXPECT_EQ(hat->ClassName, "class WizItemTemplate");
    EXPECT_EQ(hat->DisplayKey, "Items_00028316");
}

TEST_F(TemplateExtractorClientTest, BuildingTheScriptTwiceWritesTheSameRows)
{
    EXPECT_EQ(TemplateScript::Build(*s_extraction).ToText(), TemplateScript::Build(*s_extraction).ToText());
}
