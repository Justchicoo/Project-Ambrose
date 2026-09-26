/*
 * Project Ambrose by Imjustchico
 * Tests runtime discovery over synthetic heaps and code: a map head among decoy nodes, cycle refusal, constructor and initializer votes, and Type and string fields derived from chosen values at non-default offsets.
 */

#include "ClientDiscovery.h"
#include "CodeIndex.h"
#include "GuestObjects.h"
#include "PeBuilder.h"
#include "PeImage.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace
{
    constexpr uint64 HeapBase = 0x10000000000;

    struct Heap
    {
        Machine machine;
        GuestHeap heap{ machine, HeapBase, 0x1000000 };
        GuestObjects objects{ machine, heap };
    };

    void PutDisp(std::vector<uint8>& code, std::size_t at, int64 value)
    {
        uint32 const disp = static_cast<uint32>(static_cast<int32>(value));
        for (int i = 0; i < 4; ++i)
            code[at + static_cast<std::size_t>(i)] = static_cast<uint8>(disp >> (8 * i));
    }

    struct Map
    {
        uint64 head = 0;
        std::vector<uint64> types;
        std::vector<uint64> nodes;
    };

    Map BuildMap(Heap& h, std::vector<std::string_view> names)
    {
        std::sort(names.begin(), names.end(), [](std::string_view a, std::string_view b) { return StringHash::KiStringHash(a) < StringHash::KiStringHash(b); });
        Map map;
        for (std::string_view const name : names)
            map.types.push_back(h.objects.Type(name));
        map.head = h.objects.MapNode(0, 0, true);
        for (std::size_t i = 0; i < names.size(); ++i)
            map.nodes.push_back(h.objects.MapNode(StringHash::KiStringHash(names[i]), map.types[i], false));
        h.objects.Link(map.nodes[1], map.nodes[0], map.head, map.nodes[2]);
        h.objects.Link(map.nodes[0], map.head, map.nodes[1], map.head);
        h.objects.Link(map.nodes[2], map.head, map.nodes[1], map.head);
        h.objects.Link(map.head, map.nodes[0], map.nodes[1], map.nodes[2]);
        return map;
    }
}

TEST(ClientDiscoveryRuntimeTest, TheTypeMapHeadIsFoundAmongDecoysAndWalkedInOrder)
{
    Heap h;
    uint64 const decoyType = h.objects.Type("class Decoy");
    h.machine.WriteU32(decoyType + ClientLayout{}.TypeHash, 12345);
    h.objects.MapNode(12345, decoyType, false);
    Map const map = BuildMap(h, { "class Alpha", "int", "class SharedPointer<class Alpha>" });

    std::string error;
    std::optional<uint64> const head = ClientDiscovery::FindTypeMapHead(h.machine, h.heap, {}, error);
    ASSERT_TRUE(head) << error;
    EXPECT_EQ(*head, map.head);
    std::optional<std::vector<uint64>> const walked = ClientDiscovery::WalkTypeMap(h.machine, *head, {}, error);
    ASSERT_TRUE(walked) << error;
    EXPECT_EQ(*walked, map.types);

    h.objects.Link(map.nodes[2], map.nodes[1], map.nodes[1], map.head);
    EXPECT_FALSE(ClientDiscovery::WalkTypeMap(h.machine, *head, {}, error));
    EXPECT_NE(error.find("cycle"), std::string::npos) << error;
}

TEST(ClientDiscoveryRuntimeTest, AHeapWithoutTypesOrWithTwoMapsIsAnError)
{
    Heap empty;
    std::string error;
    EXPECT_FALSE(ClientDiscovery::FindTypeMapHead(empty.machine, empty.heap, {}, error));
    empty.heap.Allocate(64, true);
    EXPECT_FALSE(ClientDiscovery::FindTypeMapHead(empty.machine, empty.heap, {}, error));
    EXPECT_NE(error.find("no type map node"), std::string::npos) << error;

    Heap two;
    BuildMap(two, { "class A", "class B", "class C" });
    BuildMap(two, { "class D", "class E", "class F" });
    EXPECT_FALSE(ClientDiscovery::FindTypeMapHead(two.machine, two.heap, {}, error));
    EXPECT_NE(error.find("2 heads"), std::string::npos) << error;
}

TEST(ClientDiscoveryRuntimeTest, TheConstructorAndListInitializerWinTheirVotes)
{
    constexpr std::size_t Functions = 25;
    constexpr uint64 ImageBase = 0x140000000;
    constexpr uint32 FunctionSize = 16;
    PeBuilder builder(ImageBase, false);
    uint32 const codeRva = builder.NextSectionRva();
    uint32 const dataRva = codeRva + 0x1000;
    uint32 const constructorRva = codeRva + FunctionSize * (Functions * 2 + 4);
    uint32 const decoyRva = constructorRva + 1;
    uint32 const initializerRva = constructorRva + 2;
    std::vector<uint8> code(FunctionSize * (Functions * 2 + 4) + 3, 0xCC);
    for (std::size_t i = 0; i < Functions + 3; ++i)
    {
        uint32 const at = FunctionSize * static_cast<uint32>(i);
        uint32 const target = i < Functions ? constructorRva : decoyRva;
        std::vector<uint8> const body = { 0xE8, 0, 0, 0, 0, 0x48, 0x8D, 0x05, 0, 0, 0, 0, 0x48, 0x89, 0x03, 0xC3 };
        std::copy(body.begin(), body.end(), code.begin() + at);
        PutDisp(code, at + 1, static_cast<int64>(target) - static_cast<int64>(codeRva + at + 5));
        PutDisp(code, at + 8, static_cast<int64>(dataRva) - static_cast<int64>(codeRva + at + 12));
        builder.AddFunction(codeRva + at, codeRva + at + FunctionSize);
    }
    for (std::size_t i = 0; i < Functions; ++i)
    {
        uint32 const at = FunctionSize * static_cast<uint32>(Functions + 3 + i);
        std::vector<uint8> const body = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0xE8, 0, 0, 0, 0, 0xC3 };
        std::copy(body.begin(), body.end(), code.begin() + at);
        PutDisp(code, at + 3, static_cast<int64>(dataRva + 0x100 + 0x10 * i) - static_cast<int64>(codeRva + at + 7));
        PutDisp(code, at + 8, static_cast<int64>(initializerRva) - static_cast<int64>(codeRva + at + 12));
        builder.AddFunction(codeRva + at, codeRva + at + FunctionSize);
    }
    code[constructorRva - codeRva] = 0xC3;
    code[decoyRva - codeRva] = 0xC3;
    code[initializerRva - codeRva] = 0xC3;
    builder.AddSection(".text", code, PeBuilder::CodeCharacteristics);
    builder.AddSection(".rdata", std::vector<uint8>(0x400, 0), PeBuilder::ReadOnlyCharacteristics);
    std::string error;
    std::unique_ptr<PeImage> const image = PeImage::Parse(builder.Build(), error);
    ASSERT_NE(image, nullptr) << error;
    CodeIndex const index(*image);

    Heap h;
    std::vector<uint64> types;
    for (std::size_t i = 0; i < Functions; ++i)
        types.push_back(h.objects.Type("class T" + std::to_string(i), ImageBase + dataRva, false, ImageBase + dataRva + 0x100 + 0x10 * i));
    std::optional<DiscoveryVote> const constructor = ClientDiscovery::FindTypeConstructor(h.machine, index, types, error);
    ASSERT_TRUE(constructor) << error;
    EXPECT_EQ(constructor->Winner, ImageBase + constructorRva);
    EXPECT_EQ(constructor->WinnerVotes, Functions);
    EXPECT_EQ(constructor->RunnerUp, ImageBase + decoyRva);
    EXPECT_EQ(constructor->RunnerUpVotes, 3u);
    std::optional<DiscoveryVote> const initializer = ClientDiscovery::FindPropertyListInitializer(h.machine, index, types, {}, error);
    ASSERT_TRUE(initializer) << error;
    EXPECT_EQ(initializer->Winner, ImageBase + initializerRva);
    EXPECT_EQ(initializer->WinnerVotes, Functions);

    std::vector<uint64> const few(types.begin(), types.begin() + 5);
    EXPECT_FALSE(ClientDiscovery::FindPropertyListInitializer(h.machine, index, few, {}, error));
    EXPECT_NE(error.find("no clear winner"), std::string::npos) << error;
}

TEST(ClientDiscoveryRuntimeTest, ConstructorChosenValuesLocateTypeAndStringFieldsAwayFromDefaults)
{
    Heap h;
    std::vector<ConstructedTypeSample> samples;
    auto makeType = [&](std::string name, uint32 hash, bool inlineName)
    {
        uint64 const type = h.heap.Allocate(0x80, true);
        uint64 const nameField = type + 0x20;
        std::vector<uint8> bytes(name.begin(), name.end());
        bytes.push_back(0);
        if (inlineName)
        {
            bytes.resize(8, 0);
            h.machine.Write(nameField, bytes);
        }
        else
        {
            uint64 const storage = h.heap.Allocate(bytes.size(), true);
            h.machine.Write(storage, bytes);
            h.machine.WriteU64(nameField, storage);
        }
        h.machine.WriteU64(nameField + 0x08, name.size());
        h.machine.WriteU64(nameField + 0x10, inlineName ? 7 : name.size());
        h.machine.WriteU64(nameField + 0x40, name.size());
        h.machine.WriteU32(type + 0x38, hash);
        h.machine.WriteU32(type + 0x70, hash);
        samples.push_back({ type, std::move(name), hash });
    };
    makeType("Tiny", 0xA17B23C1, true);
    makeType("LongConstructorProbeName", 0x5C4D2E19, false);

    ClientLayout layout;
    std::string error;
    ASSERT_TRUE(ClientDiscovery::DeriveConstructedTypeLayout(h.machine, h.heap, samples, layout, error)) << error;

    EXPECT_EQ(layout.TypeName, 0x20u);
    EXPECT_EQ(layout.TypeHash, 0x38u);
    EXPECT_EQ(layout.StringSize, 0x08u);
    EXPECT_EQ(layout.StringCapacity, 0x10u);
    EXPECT_EQ(layout.StringInlineCapacity, 7u);
    EXPECT_EQ(layout.StringObjectSize, 0x18u);
    EXPECT_EQ(layout.FirstUnresolvedField(), "std::map.node.left");
}
