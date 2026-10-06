/*
 * Project Ambrose by Imjustchico
 * Tests the game effect templates on a Root.wad the test writes through a type dump it lays out: every template of every GameEffectData/ list file is found by the hash of its name, as the client finds it, with its class, duration, visibility and pet flag; the effect a template makes is its effect class carrying the template's id and the values both classes share by name and type, and travels as MSG_ADDEFFECT's EffectData; a template whose class makes no effect makes none; a name read twice keeps the first and a nameless template is left out, each with a warning; and a file that is not a template list fails a reload, which keeps the set serving.
 */

#include "GameEffectFixtures.h"
#include "GameEffectHolder.h"
#include "GameEffectMgr.h"
#include "KiwadArchive.h"
#include "LogTestDirectory.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "ReloadMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Files = std::vector<std::pair<std::string, std::vector<uint8>>>;

    bool Holds(std::vector<std::string> const& lines, std::string const& text)
    {
        return std::any_of(lines.begin(), lines.end(), [&text](std::string const& line) { return line.find(text) != std::string::npos; });
    }

    class GameEffectMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            CharacterTypeFixtures::Detail::Json dump = CharacterTypeFixtures::Detail::Json::parse(CharacterTypeFixtures::Dump());
            GameEffectFixtures::AddClasses(dump["classes"]);
            GameEffectFixtures::AddTemplateClasses(dump["classes"]);
            ASSERT_TRUE(sTypeRegistry.LoadFromText(dump.dump(), "effects.json")) << sTypeRegistry.GetErrors().front();
            _catalog = sTypeRegistry.GetCatalog();
            _effects.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            _effects.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
        }

        PropertyObjectPtr Transformation(std::string const& name)
        {
            PropertyObjectPtr made = GameEffectFixtures::Template(_catalog, "class TransformationEffectTemplate", name, 0.0, true, true);
            EXPECT_EQ(made->Set("m_sRace", std::string("Transformation_BallerinaBear")), PropertySetResult::Ok);
            EXPECT_EQ(made->Set("m_fScale", 2.0f), PropertySetResult::Ok);
            return made;
        }

        Files GoodFiles()
        {
            std::vector<PropertyObjectPtr> named;
            named.push_back(GameEffectFixtures::Template(_catalog, "class NamedEffectTemplate", "PostCombatEffect", 30.0, true, false));
            named.push_back(GameEffectFixtures::Template(_catalog, "class StateEffectTemplate", "Stunned", 5.0, false, false));
            std::vector<PropertyObjectPtr> transformations;
            transformations.push_back(Transformation("BearBallerinaTransformation"));
            return { { "GameEffectData/NamedEffects.xml", GameEffectFixtures::ListFile(_catalog, std::move(named)) },
                { "GameEffectData/Transformations.xml", GameEffectFixtures::ListFile(_catalog, std::move(transformations)) },
                { "GameEffectData/ReadMe.txt", std::vector<uint8>{ 'n', 'o' } } };
        }

        std::shared_ptr<GameEffectStore const> ReadWritten(std::vector<std::string>& errors, std::vector<std::string>& warnings)
        {
            std::string error;
            std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(_directory.Path() / "Data" / "GameData" / "Root.wad", error);
            EXPECT_TRUE(root) << error;
            return root ? GameEffectMgr::Read(*root, _catalog, errors, warnings) : nullptr;
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        TypeCatalogPtr _catalog;
        GameEffectMgr _effects;
    };
}

TEST_F(GameEffectMgrTest, EveryTemplateIsFoundByTheHashOfItsNameWithWhatItIs)
{
    GameEffectFixtures::WriteRoot(_directory.Path(), GoodFiles());
    std::vector<std::string> errors;
    ASSERT_TRUE(_effects.Load(errors)) << errors.front();
    std::shared_ptr<GameEffectStore const> const effects = _effects.GetEffects();
    ASSERT_EQ(effects->Size(), 3u) << "a file that does not end in .xml is not a list";
    GameEffectInfo const* const post = effects->Find(StringHash::KiStringHash("PostCombatEffect"));
    ASSERT_NE(post, nullptr);
    EXPECT_EQ(post->Name, "PostCombatEffect");
    EXPECT_EQ(post->File, "GameEffectData/NamedEffects.xml");
    EXPECT_EQ(post->TemplateClass, "class NamedEffectTemplate");
    EXPECT_EQ(post->EffectClass, "class NamedEffect");
    EXPECT_EQ(post->Category, "Test");
    EXPECT_DOUBLE_EQ(post->Duration, 30.0);
    EXPECT_TRUE(post->IsPublic);
    EXPECT_FALSE(post->IsOnPet);
    EXPECT_EQ(effects->FindByName("bearballerinatransformation")->EffectClass, "class TransformationEffect");
    EXPECT_TRUE(effects->FindByName("Stunned")->EffectClass.empty()) << "the dump has no StateEffect, so the template makes no effect";
    EXPECT_TRUE(Holds(post->Describe(), "makes a class NamedEffect"));
    EXPECT_TRUE(Holds(post->Describe(), "lasts 30 s, seen by everyone, shown on the wizard, category Test"));
}

TEST_F(GameEffectMgrTest, AnEffectCarriesItsTemplatesIdAndTheValuesBothClassesShare)
{
    GameEffectFixtures::WriteRoot(_directory.Path(), GoodFiles());
    std::vector<std::string> errors;
    ASSERT_TRUE(_effects.Load(errors)) << errors.front();
    std::shared_ptr<GameEffectStore const> const effects = _effects.GetEffects();
    std::string problem;
    PropertyObjectPtr bear = effects->FindByName("BearBallerinaTransformation")->MakeEffect(problem);
    ASSERT_TRUE(bear) << problem;
    EXPECT_TRUE(bear->IsA("class TransformationEffect"));
    EXPECT_EQ(*bear->Get("m_effectNameID")->GetIf<uint32>(), StringHash::KiStringHash("BearBallerinaTransformation"));
    EXPECT_EQ(*bear->Get("m_sRace")->GetIf<std::string>(), "Transformation_BallerinaBear");
    EXPECT_TRUE(*bear->Get("m_bIsOnPet")->GetIf<bool>()) << "an effect on the pet says so";
    EXPECT_DOUBLE_EQ(*bear->Get("m_fScale")->GetIf<double>(), 0.0) << "a value the effect holds as another type is left at its default";

    EXPECT_FALSE(effects->FindByName("Stunned")->MakeEffect(problem));
    EXPECT_EQ(problem, "Stunned is a class StateEffectTemplate, and the type dump has no effect class for it");

    GameEffectHolder holder;
    PropertyObjectPtr post = effects->FindByName("PostCombatEffect")->MakeEffect(problem);
    ASSERT_TRUE(post) << problem;
    ASSERT_TRUE(holder.Add(std::move(post), problem)) << problem;
    ObjectField const* const field = ObjectFields::Find("MSG_ADDEFFECT", "EffectData");
    ASSERT_NE(field, nullptr);
    EncodeResult const data = ObjectSerializer::EncodeField(*field, holder.Find(1)->Effect.get());
    ASSERT_TRUE(data.Ok()) << data.Detail;
    DecodeResult const back = ObjectSerializer::DecodeField(_catalog, *field, data.Bytes);
    ASSERT_TRUE(back.Ok() && back.Object) << back.Detail;
    EXPECT_EQ(*back.Object->Get("m_effectNameID")->GetIf<uint32>(), StringHash::KiStringHash("PostCombatEffect"));
    EXPECT_EQ(*back.Object->Get("m_internalID")->GetIf<int32>(), 1);
}

TEST_F(GameEffectMgrTest, ANameReadTwiceKeepsTheFirstAndANamelessTemplateIsLeftOut)
{
    std::vector<PropertyObjectPtr> first;
    first.push_back(GameEffectFixtures::Template(_catalog, "class NamedEffectTemplate", "PostCombatEffect", 30.0, true, false));
    std::vector<PropertyObjectPtr> second;
    second.push_back(GameEffectFixtures::Template(_catalog, "class NamedEffectTemplate", "PostCombatEffect", 99.0, true, false));
    second.push_back(GameEffectFixtures::Template(_catalog, "class NamedEffectTemplate", "", 1.0, true, false));
    GameEffectFixtures::WriteRoot(_directory.Path(), { { "GameEffectData/B.xml", GameEffectFixtures::ListFile(_catalog, std::move(second)) },
        { "GameEffectData/A.xml", GameEffectFixtures::ListFile(_catalog, std::move(first)) } });
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::shared_ptr<GameEffectStore const> const effects = ReadWritten(errors, warnings);
    ASSERT_TRUE(effects) << errors.front();
    ASSERT_EQ(effects->Size(), 1u);
    EXPECT_DOUBLE_EQ(effects->FindByName("PostCombatEffect")->Duration, 30.0) << "files are read in name order, and the first is kept";
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_TRUE(Holds(warnings, "PostCombatEffect in GameEffectData/B.xml was read before in GameEffectData/A.xml"));
    EXPECT_TRUE(Holds(warnings, "template 1 of GameEffectData/B.xml has no m_effectName"));
}

TEST_F(GameEffectMgrTest, AFileThatIsNotATemplateListFailsAReloadWhichKeepsTheSetServing)
{
    _effects.RegisterReloadTargets();
    GameEffectFixtures::WriteRoot(_directory.Path(), GoodFiles());
    ASSERT_TRUE(sReloadMgr.Reload(std::string(GameEffectMgr::Target)).Ok);
    EXPECT_EQ(_effects.GetEffects()->Size(), 3u);

    Files files = GoodFiles();
    PropertyObjectPtr stray = GameEffectFixtures::Template(_catalog, "class NamedEffectTemplate", "Stray", 1.0, true, false);
    EncodeResult encoded = BindFile::Write(stray.get());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    files.emplace_back("GameEffectData/Stray.xml", std::move(encoded.Bytes));
    files.emplace_back("GameEffectData/Broken.xml", std::vector<uint8>{ 'b', 'r', 'o', 'k', 'e', 'n' });
    GameEffectFixtures::WriteRoot(_directory.Path(), files);
    ReloadOutcome const failed = sReloadMgr.Reload(std::string(GameEffectMgr::Target));
    EXPECT_FALSE(failed.Ok);
    EXPECT_TRUE(Holds(failed.Errors, "GameEffectData/Stray.xml is a class NamedEffectTemplate, which is not a class GameEffectTemplateList"));
    EXPECT_TRUE(Holds(failed.Errors, "GameEffectData/Broken.xml in Root.wad does not read"));
    EXPECT_EQ(_effects.GetEffects()->Size(), 3u) << "the set that was serving goes on serving";

    GameEffectFixtures::WriteRoot(_directory.Path(), { { "Other/Thing.xml", std::vector<uint8>{ 'x' } } });
    std::vector<std::string> errors;
    EXPECT_FALSE(_effects.Load(errors));
    EXPECT_TRUE(Holds(errors, "holds no .xml file under GameEffectData/"));
}
