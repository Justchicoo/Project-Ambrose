/*
 * Project Ambrose by Imjustchico
 * Loads the user's own r806919 type dump, when AMBROSE_TYPE_DUMP_PATH names it, into a registry with every built-in typed view, checks each binds, builds a view over a default object of each view's class and reads every accessor, and checks views over a derived object, the core template view over game object, item and recipe templates alike, and refusals over an unrelated one.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectViews.h"

#include <gtest/gtest.h>

#include <string>

TEST(TypedViewClientTest, EveryBuiltInViewBindsAgainstR806919AndReadsEveryField)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump (format v2) from your own client to run this test";

    TypeRegistry registry(&sTypedViewRegistry);
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path))) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();
    std::vector<ViewDefinition const*> const views = sTypedViewRegistry.GetViews();
    ASSERT_EQ(views.size(), 11u);
    for (ViewDefinition const* view : views)
    {
        ViewBinding const* const binding = catalog->FindView(*view);
        ASSERT_NE(binding, nullptr) << view->Name;
        EXPECT_EQ(binding->Ordinals.size(), view->Fields.size()) << view->Name;
    }

    auto const create = [&catalog](std::string_view className)
    {
        PropertyObjectPtr object = PropertyObject::Create(catalog, className);
        EXPECT_TRUE(object) << className;
        return object;
    };

    PropertyObjectPtr const creation = create("class WizardCharacterCreationInfo");
    ASSERT_TRUE(creation);
    std::optional<WizardCharacterCreationInfoView> const info = WizardCharacterCreationInfoView::From(*creation);
    ASSERT_TRUE(info);
    EXPECT_EQ(info->GetTemplateId(), 0);
    EXPECT_TRUE(info->GetName().empty());
    EXPECT_FALSE(info->ShouldRename());
    EXPECT_EQ(info->GetGlobalId(), 0u);
    EXPECT_EQ(info->GetUserId(), 0u);
    EXPECT_FALSE(info->IsQuarantined());
    EXPECT_EQ(info->GetLastLoginTime(), 0u);
    EXPECT_EQ(info->GetAvatarBehavior(), nullptr);
    EXPECT_EQ(info->GetEquipmentInfoList(), nullptr);
    EXPECT_TRUE(info->GetLocation().empty());
    EXPECT_EQ(info->GetLevel(), 0);
    EXPECT_EQ(info->GetWorld(), 0);
    EXPECT_EQ(info->GetSchoolOfFocus(), 0u);
    EXPECT_EQ(info->GetNameIndices(), 0u);

    PropertyObjectPtr const player = create("class WizClientObject");
    ASSERT_TRUE(player);
    std::optional<CoreObjectView> const core = CoreObjectView::From(*player);
    ASSERT_TRUE(core);
    EXPECT_TRUE(core->GetInactiveBehaviors().empty());
    EXPECT_EQ(core->GetGlobalId(), 0u);
    EXPECT_EQ(core->GetPermId(), 0u);
    EXPECT_EQ(core->GetLocation(), PropertyTypes::Vector3D{});
    EXPECT_EQ(core->GetOrientation(), PropertyTypes::Vector3D{});
    EXPECT_EQ(core->GetScale(), 0.0f);
    EXPECT_EQ(core->GetTemplateId(), 0u);
    EXPECT_TRUE(core->GetDebugName().empty());
    EXPECT_TRUE(core->GetDisplayKey().empty());
    EXPECT_EQ(core->GetZoneTagId(), 0u);
    EXPECT_EQ(core->GetSpeedMultiplier(), 0);
    EXPECT_EQ(core->GetMobileId(), 0u);
    std::optional<ClientObjectView> const client = ClientObjectView::From(*player);
    ASSERT_TRUE(client);
    EXPECT_EQ(client->GetCharacterId(), 0u);
    std::optional<WizClientObjectView> const wizard = WizClientObjectView::From(*player);
    ASSERT_TRUE(wizard);
    EXPECT_EQ(wizard->GetGameStats(), nullptr);
    EXPECT_FALSE(WizItemTemplateView::From(*player));

    PropertyObjectPtr const hat = create("class WizItemTemplate");
    ASSERT_TRUE(hat);
    ASSERT_EQ(hat->Set("m_templateID", uint32{ 1652259 }), PropertySetResult::Ok);
    std::optional<GameObjectTemplateView> const object = GameObjectTemplateView::From(*hat);
    ASSERT_TRUE(object);
    EXPECT_TRUE(object->GetBehaviors().empty());
    EXPECT_TRUE(object->GetObjectName().empty());
    EXPECT_EQ(object->GetTemplateId(), 1652259u);
    EXPECT_EQ(object->GetVisualId(), 0u);
    EXPECT_TRUE(object->GetAdjectiveList().empty());
    EXPECT_FALSE(object->IsExemptFromAoi());
    EXPECT_TRUE(object->GetDisplayName().empty());
    EXPECT_TRUE(object->GetDescription().empty());
    EXPECT_EQ(object->GetObjectType(), 0);
    EXPECT_TRUE(object->GetIcon().empty());
    std::optional<WizItemTemplateView> const item = WizItemTemplateView::From(*hat);
    ASSERT_TRUE(item);
    EXPECT_EQ(item->GetTemplateId(), 1652259u);
    EXPECT_EQ(item->GetEquipRequirements(), nullptr);
    EXPECT_EQ(item->GetPurchaseRequirements(), nullptr);
    EXPECT_TRUE(item->GetEquipEffects().empty());
    EXPECT_EQ(item->GetBaseCost(), 0.0f);
    EXPECT_EQ(item->GetCreditsCost(), 0.0f);
    EXPECT_EQ(item->GetAvatarInfo(), nullptr);
    EXPECT_TRUE(item->GetAvatarFlags().empty());
    EXPECT_EQ(item->GetItemLimit(), 0);
    EXPECT_TRUE(item->GetHolidayFlag().empty());
    EXPECT_EQ(item->GetItemSetBonusTemplateId(), 0u);
    EXPECT_TRUE(item->GetSchool().empty());
    EXPECT_EQ(item->GetArenaPointCost(), 0);
    EXPECT_EQ(item->GetPvpCurrencyCost(), 0);
    EXPECT_EQ(item->GetPvpTourneyCurrencyCost(), 0);
    EXPECT_EQ(item->GetRank(), 0);
    EXPECT_EQ(item->GetRarity(), 0);

    PropertyObjectPtr const coreTemplate = create("class CoreTemplate");
    ASSERT_TRUE(coreTemplate);
    std::optional<CoreTemplateView> const coreView = CoreTemplateView::From(*coreTemplate);
    ASSERT_TRUE(coreView);
    EXPECT_TRUE(coreView->GetBehaviors().empty());
    PropertyObjectPtr const recipe = create("class RecipeTemplate");
    ASSERT_TRUE(recipe);
    EXPECT_TRUE(CoreTemplateView::From(*recipe)) << "a recipe is a template without being a game object";
    EXPECT_FALSE(GameObjectTemplateView::From(*recipe));
    EXPECT_TRUE(CoreTemplateView::From(*hat));
    EXPECT_FALSE(CoreTemplateView::From(*player)) << "an object in the world is not a template";

    PropertyObjectPtr const manifest = create("class TemplateManifest");
    ASSERT_TRUE(manifest);
    std::optional<TemplateManifestView> const manifestView = TemplateManifestView::From(*manifest);
    ASSERT_TRUE(manifestView);
    EXPECT_TRUE(manifestView->GetSerializedTemplates().empty());
    EXPECT_FALSE(CoreTemplateView::From(*manifest));

    PropertyObjectPtr const location = create("class TemplateLocation");
    ASSERT_TRUE(location);
    ASSERT_EQ(location->Set("m_filename", "ObjectData/Hat.xml"), PropertySetResult::Ok);
    std::optional<TemplateLocationView> const locationView = TemplateLocationView::From(*location);
    ASSERT_TRUE(locationView);
    EXPECT_EQ(locationView->GetFilename(), "ObjectData/Hat.xml");
    EXPECT_EQ(locationView->GetId(), 0u);

    PropertyObjectPtr const requirements = create("class RequirementList");
    ASSERT_TRUE(requirements);
    std::optional<RequirementListView> const requirementsView = RequirementListView::From(*requirements);
    ASSERT_TRUE(requirementsView);
    EXPECT_FALSE(requirementsView->AppliesNot());
    EXPECT_EQ(requirementsView->GetOperator(), 0);
    EXPECT_TRUE(requirementsView->GetRequirements().empty());

    PropertyObjectPtr const effect = create("class NamedEffect");
    ASSERT_TRUE(effect);
    std::optional<NamedEffectView> const effectView = NamedEffectView::From(*effect);
    ASSERT_TRUE(effectView);
    EXPECT_EQ(effectView->GetCurrentTickCount(), 0.0);
    EXPECT_EQ(effectView->GetEffectNameId(), 0u);
    EXPECT_FALSE(effectView->IsOnPet());
    EXPECT_EQ(effectView->GetOriginatorId(), 0u);
    EXPECT_EQ(effectView->GetItemSlotId(), 0u);
    EXPECT_EQ(effectView->GetInternalId(), 0);
    EXPECT_EQ(effectView->GetEndTime(), 0u);
    EXPECT_TRUE(effectView->GetOverrideName().empty());
}
