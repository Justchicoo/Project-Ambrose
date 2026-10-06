/*
 * Project Ambrose by Imjustchico
 * Tests the effects one object carries, on classes the test lays out the way the client's dump gives them: each effect gets the lowest internal id no effect on the object holds, written into its m_internalID, ids never repeat while their effects are kept and come free when one is taken away, an object that is not a GameEffectBase is refused, the container an effect behavior carries is never null and lists a copy of each effect under m_publicEffects, and an effect reads back equal through MSG_ADDEFFECT's EffectData.
 */

#include "GameEffectFixtures.h"
#include "GameEffectHolder.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

namespace
{
    TypeCatalogPtr LoadCatalog()
    {
        CharacterTypeFixtures::Detail::Json dump = CharacterTypeFixtures::Detail::Json::parse(CharacterTypeFixtures::Dump());
        GameEffectFixtures::AddClasses(dump["classes"]);
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(dump.dump(), "effects.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    PropertyObjectPtr MakeEffect(TypeCatalogPtr const& catalog, uint32 nameId)
    {
        PropertyObjectPtr effect = PropertyObject::Create(catalog, "class NamedEffect");
        EXPECT_TRUE(effect);
        if (effect)
        {
            EXPECT_EQ(effect->Set("m_effectNameID", nameId), PropertySetResult::Ok);
        }
        return effect;
    }

    int32 InternalIdOf(ActiveGameEffect const& active)
    {
        int32 const* const id = active.Effect->Get("m_internalID")->GetIf<int32>();
        return id != nullptr ? *id : -1;
    }
}

TEST(GameEffectHolderTest, InternalIdsAreUniquePerObjectAndReleasedOnRemoval)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    GameEffectHolder holder;
    std::string problem;
    std::vector<int32> ids;
    for (uint32 nameId : { 11u, 22u, 33u, 22u })
    {
        std::optional<int32> const id = holder.Add(MakeEffect(catalog, nameId), problem);
        ASSERT_TRUE(id) << problem;
        ids.push_back(*id);
    }
    EXPECT_EQ(ids, (std::vector<int32>{ 1, 2, 3, 4 })) << "two effects of the same name still get ids of their own";
    for (ActiveGameEffect const& active : holder.GetEffects())
        EXPECT_EQ(InternalIdOf(active), active.InternalId) << "the id is written into the effect the client receives";

    std::optional<ActiveGameEffect> const removed = holder.Remove(2);
    ASSERT_TRUE(removed);
    EXPECT_EQ(removed->EffectNameId, 22u);
    EXPECT_EQ(holder.Find(2), nullptr);
    EXPECT_FALSE(holder.Remove(2)) << "an id is taken away once";
    EXPECT_FALSE(holder.Remove(9));

    std::optional<int32> const reused = holder.Add(MakeEffect(catalog, 44), problem);
    ASSERT_TRUE(reused) << problem;
    EXPECT_EQ(*reused, 2) << "the released id is the lowest free one again";
    std::optional<int32> const next = holder.Add(MakeEffect(catalog, 55), problem);
    ASSERT_TRUE(next) << problem;
    EXPECT_EQ(*next, 5);

    std::set<int32> seen;
    for (ActiveGameEffect const& active : holder.GetEffects())
        EXPECT_TRUE(seen.insert(active.InternalId).second) << active.InternalId;
    EXPECT_EQ(seen.size(), 5u);
    ASSERT_NE(holder.Find(2), nullptr);
    EXPECT_EQ(holder.Find(2)->EffectNameId, 44u);
}

TEST(GameEffectHolderTest, OnlyAGameEffectIsKept)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    GameEffectHolder holder;
    std::string problem;
    EXPECT_FALSE(holder.Add(PropertyObject::Create(catalog, "class WizardCharacterBehavior"), problem));
    EXPECT_EQ(problem, "class WizardCharacterBehavior is not a class GameEffectBase");
    problem.clear();
    EXPECT_FALSE(holder.Add(nullptr, problem));
    EXPECT_FALSE(problem.empty());
    EXPECT_EQ(holder.Count(), 0u);
}

TEST(GameEffectHolderTest, TheBehaviorCarriesAContainerThatIsNeverNull)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr const behavior = PropertyObject::Create(catalog, "class BaseGameEffectBehavior");
    ASSERT_TRUE(behavior);
    EXPECT_TRUE(behavior->Get("m_gameEffects")->IsNullObject()) << "the class's default leaves it null, which the client's MSG_AddEffect handler would write through";
    std::string problem;
    ASSERT_TRUE(GameEffectHolder::FillBehavior(*behavior, problem)) << problem;
    PropertyObject const* const container = behavior->Get("m_gameEffects")->AsObject();
    ASSERT_NE(container, nullptr);
    EXPECT_TRUE(container->IsA(GameEffectHolder::ContainerClass));
    EXPECT_TRUE(container->Get("m_publicEffects")->GetList()->empty());

    GameEffectHolder holder;
    ASSERT_TRUE(holder.Add(MakeEffect(catalog, 7), problem)) << problem;
    ASSERT_TRUE(holder.Add(MakeEffect(catalog, 8), problem)) << problem;
    PropertyObjectPtr const built = holder.BuildContainer(catalog, problem);
    ASSERT_TRUE(built) << problem;
    PropertyValue::List const& listed = *built->Get("m_publicEffects")->GetList();
    ASSERT_EQ(listed.size(), 2u);
    EXPECT_EQ(*listed[0].AsObject()->Get("m_effectNameID")->GetIf<uint32>(), 7u);
    EXPECT_EQ(*listed[1].AsObject()->Get("m_internalID")->GetIf<int32>(), 2);
}

TEST(GameEffectHolderTest, AnEffectReadsBackEqualThroughAddEffectsData)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    ObjectField const* const field = ObjectFields::Find("MSG_ADDEFFECT", "EffectData");
    ASSERT_NE(field, nullptr);
    EXPECT_FALSE(field->Enveloped);
    EXPECT_FALSE(field->CoreObjects);

    GameEffectHolder holder;
    std::string problem;
    PropertyObjectPtr effect = MakeEffect(catalog, 1234567);
    ASSERT_EQ(effect->Set("m_overrideName", "Test"), PropertySetResult::Ok);
    ASSERT_EQ(effect->Set("m_originatorID", uint64{ 99 }), PropertySetResult::Ok);
    ASSERT_TRUE(holder.Add(std::move(effect), problem)) << problem;
    EncodeResult const data = ObjectSerializer::EncodeField(*field, holder.Find(1)->Effect.get());
    ASSERT_TRUE(data.Ok()) << data.Detail;
    DecodeResult const back = ObjectSerializer::DecodeField(catalog, *field, data.Bytes);
    ASSERT_TRUE(back.Ok() && back.Object) << back.Detail;
    EXPECT_TRUE(back.Object->IsA("class NamedEffect"));
    EXPECT_EQ(*back.Object->Get("m_effectNameID")->GetIf<uint32>(), 1234567u);
    EXPECT_EQ(*back.Object->Get("m_internalID")->GetIf<int32>(), 1);
    EXPECT_EQ(*back.Object->Get("m_overrideName")->GetIf<std::string>(), "Test");
    EXPECT_EQ(*back.Object->Get("m_originatorID")->GetIf<uint64>(), 0u) << "the originator is not transmitted";

    PropertyObjectPtr const other = PropertyObject::Create(catalog, "class WizardCharacterBehavior");
    EXPECT_FALSE(ObjectSerializer::EncodeField(*field, other.get()).Ok()) << "the field carries only a game effect";
}
