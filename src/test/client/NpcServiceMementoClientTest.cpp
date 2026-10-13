/*
 * Project Ambrose by Imjustchico
 * Uses the user's client type dump to check that the ServiceMementoBase MSG_SENDNPCOPTIONS carries is the bare object, its class hash first, as the client reads it.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "NpcServiceMemento.h"
#include "NpcServiceMenu.h"
#include "ObjectSerializer.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class OneOptionProvider : public NpcServiceProvider
    {
    public:
        std::string_view GetName() const override { return "one"; }

        std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const override
        {
            (void)target;
            return { { "Talk", "", "Talk_Key" } };
        }

        void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) override
        {
            (void)target;
            (void)index;
        }
    };
}

TEST(NpcServiceMementoClientTest, TheMenuIsSentAsTheBareObjectWithItsClassHashFirst)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to a type dump from your own client to run this test";

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path)))
        << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_NE(catalog, nullptr);
    ClassInfo const* const memento = catalog->FindClass("class ServiceMementoBase");
    ASSERT_NE(memento, nullptr);

    OneOptionProvider provider;
    std::vector<NpcServiceProvider*> const providers{ &provider };
    NpcServiceMenu const menu = NpcServiceMenu::Build(providers, { 7, 1001, 38232 });
    QuestWireEncoder::BlobEncodeResult const encoded = NpcServiceMemento::Encode(catalog, menu, { "WC-NPCs_00000125", "GUI/NpcPortraits/Art_Portrait_Boy_Fire.dds" });
    ASSERT_TRUE(encoded.Ok()) << encoded.Error;
    ASSERT_GE(encoded.Bytes.size(), 4u);

    uint32 first = 0;
    std::memcpy(&first, encoded.Bytes.data(), sizeof(first));
    EXPECT_EQ(first, memento->Hash);

    SerializerOptions options;
    options.Mask = QuestWireEncoder::ServiceMementoMask;
    DecodeResult const decoded = ObjectSerializer::Decode(catalog, encoded.Bytes, options);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_NE(decoded.Object, nullptr);
    EXPECT_EQ(decoded.Object->GetClass().Hash, memento->Hash);
}
