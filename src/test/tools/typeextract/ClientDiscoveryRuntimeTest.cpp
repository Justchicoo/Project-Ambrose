/*
 * Project Ambrose by Imjustchico
 * Tests runtime discovery over synthetic heaps and code: std::map node fields derived from the tree's shape at default and shifted offsets, the property list link and pointer flag derived from registered types, PropertyList base, singleton and name placed by chosen constructor values, the property vector and Property name, type and hash voted on by property hashes, Property id from list positions and the container slot and vtable entries from what containers return, a map head among decoy nodes, cycle refusal, constructor and initializer votes, and Type and string fields derived from chosen values at non-default offsets.
 */

#include "ClientDiscovery.h"
#include "CodeIndex.h"
#include "GuestObjects.h"
#include "PeBuilder.h"
#include "PeImage.h"
#include "TypeWalker.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>

namespace
{
    constexpr uint64 HeapBase = 0x10000000000;

    struct Heap
    {
        explicit Heap(ClientLayout layout = {}) : objects(machine, heap, layout)
        {
        }

        Machine machine;
        GuestHeap heap{ machine, HeapBase, 0x1000000 };
        GuestObjects objects;
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

TEST(ClientDiscoveryRuntimeTest, TheMapNodeLayoutIsDerivedFromTheTreeAtDefaultAndShiftedOffsets)
{
    ClientLayout shifted;
    shifted.MapNodeLeft = 0x10;
    shifted.MapNodeParent = 0x00;
    shifted.MapNodeRight = 0x08;
    shifted.MapNodeColor = 0x1A;
    shifted.MapNodeIsNil = 0x1B;
    shifted.MapNodeKey = 0x1C;
    shifted.MapNodeValue = 0x20;
    for (ClientLayout const& expected : { ClientLayout{}, shifted })
    {
        Heap h(expected);
        uint64 const decoyType = h.objects.Type("class Decoy");
        h.objects.MapNode(StringHash::KiStringHash("class Decoy"), decoyType, false);
        Map const unnamed = BuildMap(h, { "class X", "class Y", "class Z" });
        for (uint64 const type : unnamed.types)
            h.objects.WriteString(type + expected.TypeName, "not its name");
        Map const map = BuildMap(h, { "class Alpha", "int", "class SharedPointer<class Alpha>" });

        ClientLayout derived;
        derived.MapNodeLeft = derived.MapNodeParent = derived.MapNodeRight = derived.MapNodeColor = derived.MapNodeIsNil = derived.MapNodeKey = derived.MapNodeValue = 0x99;
        std::string error;
        std::optional<uint64> const head = ClientDiscovery::DeriveTypeMapLayout(h.machine, h.heap, derived, error);
        ASSERT_TRUE(head) << error;
        EXPECT_EQ(*head, map.head);
        EXPECT_EQ(derived.MapNodeLeft, expected.MapNodeLeft);
        EXPECT_EQ(derived.MapNodeParent, expected.MapNodeParent);
        EXPECT_EQ(derived.MapNodeRight, expected.MapNodeRight);
        EXPECT_EQ(derived.MapNodeColor, expected.MapNodeColor);
        EXPECT_EQ(derived.MapNodeIsNil, expected.MapNodeIsNil);
        EXPECT_EQ(derived.MapNodeKey, expected.MapNodeKey);
        EXPECT_EQ(derived.MapNodeValue, expected.MapNodeValue);
        for (ClientLayoutEvidence const& evidence : derived.Evidence())
            EXPECT_EQ(evidence.Status, evidence.Field.starts_with("std::map.") ? "derived" : "assumed") << evidence.Field;

        std::optional<uint64> const found = ClientDiscovery::FindTypeMapHead(h.machine, h.heap, derived, error);
        ASSERT_TRUE(found) << error;
        EXPECT_EQ(*found, map.head);
        std::optional<std::vector<uint64>> const walked = ClientDiscovery::WalkTypeMap(h.machine, *found, derived, error);
        ASSERT_TRUE(walked) << error;
        EXPECT_EQ(*walked, map.types);
    }
}

TEST(ClientDiscoveryRuntimeTest, AMapLayoutWithNoTreeOrTwoEqualTreesIsRefusedNamingTheField)
{
    std::string error;
    ClientLayout layout;
    Heap empty;
    empty.heap.Allocate(64, true);
    EXPECT_FALSE(ClientDiscovery::DeriveTypeMapLayout(empty.machine, empty.heap, layout, error));
    EXPECT_NE(error.find("std::map.node.left could not be placed"), std::string::npos) << error;

    Heap broken;
    Map const map = BuildMap(broken, { "class A", "class B", "class C" });
    broken.machine.WriteU32(map.types[0] + ClientLayout{}.TypeHash, 1);
    EXPECT_FALSE(ClientDiscovery::DeriveTypeMapLayout(broken.machine, broken.heap, layout, error));
    EXPECT_NE(error.find("std::map.node.left could not be placed"), std::string::npos) << error;

    Heap two;
    BuildMap(two, { "class A", "class B", "class C" });
    BuildMap(two, { "class D", "class E", "class F" });
    EXPECT_FALSE(ClientDiscovery::DeriveTypeMapLayout(two.machine, two.heap, layout, error));
    EXPECT_NE(error.find("two std::maps of 3 nodes"), std::string::npos) << error;
    EXPECT_TRUE(layout.DerivedFields.empty());
}

TEST(ClientDiscoveryRuntimeTest, ThePropertyListLinkAndPointerFlagAreDerivedAtShiftedOffsets)
{
    ClientLayout shifted;
    shifted.TypePointer = 0x70;
    shifted.TypePropertyList = 0xA0;
    shifted.ListName = 0x80;
    Heap h(shifted);
    std::vector<uint64> types;
    for (int i = 0; i < 24; ++i)
    {
        std::string const name = fmt::format("class T{}", i);
        types.push_back(h.objects.Type(name, 0x140010000, false, h.objects.List(TypeWalker::ListNameOf(name), 0)));
        if (i % 6 == 0)
        {
            types.push_back(h.objects.Type(name + "*", 0x140010000, true));
            types.push_back(h.objects.Type("class SharedPointer<" + name + ">", 0x140010000, true));
        }
    }
    types.push_back(h.objects.Type("int"));

    ClientLayout derived;
    derived.TypePointer = derived.TypePropertyList = derived.ListName = 0x99;
    std::string error;
    ASSERT_TRUE(ClientDiscovery::DerivePropertyListLink(h.machine, h.heap, types, derived, error)) << error;
    ASSERT_TRUE(ClientDiscovery::DeriveTypePointerFlag(h.machine, h.heap, types, derived, error)) << error;
    EXPECT_EQ(derived.TypePropertyList, shifted.TypePropertyList);
    EXPECT_EQ(derived.ListName, shifted.ListName);
    EXPECT_EQ(derived.TypePointer, shifted.TypePointer);
    for (ClientLayoutEvidence const& evidence : derived.Evidence())
    {
        if (evidence.Field == "Type.pointer" || evidence.Field == "Type.property_list" || evidence.Field == "PropertyList.name")
        {
            EXPECT_EQ(evidence.Status, "derived") << evidence.Field;
        }
    }

    std::vector<uint64> const fewLists(types.begin(), types.begin() + 6);
    ClientLayout refused;
    EXPECT_FALSE(ClientDiscovery::DerivePropertyListLink(h.machine, h.heap, fewLists, refused, error));
    EXPECT_NE(error.find("Type.property_list could not be placed"), std::string::npos) << error;
    std::vector<uint64> noPointers;
    std::copy_if(types.begin(), types.end(), std::back_inserter(noPointers), [&](uint64 type) { return h.machine.ReadU8(type + shifted.TypePointer) == 0; });
    EXPECT_FALSE(ClientDiscovery::DeriveTypePointerFlag(h.machine, h.heap, noPointers, refused, error));
    EXPECT_NE(error.find("Type.pointer could not be placed"), std::string::npos) << error;
    EXPECT_TRUE(refused.DerivedFields.empty());
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

TEST(ClientDiscoveryRuntimeTest, ListConstructorChosenValuesLocateBaseSingletonAndNameAwayFromDefaults)
{
    ClientLayout shifted;
    shifted.ListBase = 0x30;
    shifted.ListSingleton = 0x0B;
    shifted.ListName = 0x60;
    Heap h(shifted);
    std::vector<ConstructedListSample> samples;
    for (int index = 0; index < 4; ++index)
    {
        ConstructedListSample& sample = samples.emplace_back();
        sample.Address = h.heap.Allocate(0x200, true);
        sample.Base = h.heap.Allocate(0x200, true);
        sample.Singleton = index % 2 == 0;
        sample.Name = "AmbroseListProbe";
        h.machine.WriteU64(sample.Address, 0x140020000);
        h.machine.WriteU8(sample.Address + shifted.ListSingleton, sample.Singleton ? 1 : 0);
        h.machine.WriteU64(sample.Address + shifted.ListBase, sample.Base);
        h.objects.WriteString(sample.Address + shifted.ListName, sample.Name);
    }

    ClientLayout derived;
    std::string error;
    ASSERT_TRUE(ClientDiscovery::DeriveConstructedListLayout(h.machine, samples, derived, error)) << error;
    EXPECT_EQ(derived.ListBase, shifted.ListBase);
    EXPECT_EQ(derived.ListSingleton, shifted.ListSingleton);
    EXPECT_EQ(derived.ListName, shifted.ListName);
    for (std::string_view const field : { "PropertyList.base", "PropertyList.singleton", "PropertyList.name" })
    {
        EXPECT_TRUE(derived.DerivedFields.contains(std::string(field))) << field;
    }

    ClientLayout disagreeing;
    disagreeing.ConfirmDerived("PropertyList.name", "registered lists");
    disagreeing.ListName = 0xB8;
    EXPECT_FALSE(ClientDiscovery::DeriveConstructedListLayout(h.machine, samples, disagreeing, error));
    EXPECT_NE(error.find("the registered lists hold it at 0xb8"), std::string::npos) << error;

    std::vector<ConstructedListSample> sameFlag(samples.begin(), samples.end());
    for (ConstructedListSample& sample : sameFlag)
        sample.Singleton = true;
    ClientLayout refused;
    EXPECT_FALSE(ClientDiscovery::DeriveConstructedListLayout(h.machine, sameFlag, refused, error));
    EXPECT_NE(error.find("with and without the singleton flag"), std::string::npos) << error;
    EXPECT_TRUE(refused.DerivedFields.empty());
}

TEST(ClientDiscoveryRuntimeTest, ThePropertyVectorAndNameTypeAndHashAreDerivedAtShiftedOffsets)
{
    ClientLayout shifted;
    shifted.ListProperties = 0x70;
    shifted.ListEntrySize = 0x18;
    shifted.PropertyName = 0x10;
    shifted.PropertyType = 0x28;
    shifted.PropertyHash = 0x34;
    Heap h(shifted);
    uint64 const intType = h.objects.Type("int");
    uint64 const localType = h.objects.Type("enum eLocal");
    std::vector<uint64> types = { intType };
    for (int i = 0; i < 24; ++i)
    {
        std::string const name = fmt::format("class P{}", i);
        uint64 const list = h.objects.List(TypeWalker::ListNameOf(name), 0);
        h.objects.SetProperties(list, { h.objects.Property(intType, "int", fmt::format("m_count{}", i), 0, 0x48, 31, 0),
            h.objects.Property(localType, "enum eLocal", "m_kind", 1, 0x4C, 7, 0) });
        types.push_back(h.objects.Type(name, 0x140010000, false, list));
    }

    ClientLayout derived;
    derived.ListProperties = derived.ListEntrySize = derived.PropertyName = derived.PropertyType = derived.PropertyHash = 0x99;
    std::string error;
    ASSERT_TRUE(ClientDiscovery::DerivePropertyLayout(h.machine, h.heap, types, derived, error)) << error;
    EXPECT_EQ(derived.ListProperties, shifted.ListProperties);
    EXPECT_EQ(derived.ListEntrySize, shifted.ListEntrySize);
    EXPECT_EQ(derived.PropertyName, shifted.PropertyName);
    EXPECT_EQ(derived.PropertyType, shifted.PropertyType);
    EXPECT_EQ(derived.PropertyHash, shifted.PropertyHash);
    for (std::string_view const field : { "PropertyList.properties", "PropertyList.entry_size", "Property.name", "Property.type", "Property.hash" })
    {
        EXPECT_TRUE(derived.DerivedFields.contains(std::string(field))) << field;
    }

    for (std::size_t index = 1; index < types.size(); ++index)
    {
        uint64 const list = h.machine.ReadU64(types[index] + shifted.TypePropertyList);
        uint64 const property = h.machine.ReadU64(h.machine.ReadU64(list + shifted.ListProperties));
        h.machine.WriteU32(property + shifted.PropertyHash, 1);
    }
    ClientLayout refused;
    EXPECT_FALSE(ClientDiscovery::DerivePropertyLayout(h.machine, h.heap, types, refused, error));
    EXPECT_NE(error.find("PropertyList.properties could not be placed"), std::string::npos) << error;
    EXPECT_TRUE(refused.DerivedFields.empty());
}

TEST(ClientDiscoveryRuntimeTest, PropertyIdsAndContainersAreDerivedAtShiftedOffsetsAndVtableEntries)
{
    ClientLayout shifted;
    shifted.PropertyId = 0x24;
    shifted.PropertyContainer = 0x30;
    shifted.ContainerNameSlot = 2;
    shifted.ContainerDynamicSlot = 5;
    Heap h(shifted);
    constexpr uint64 FunctionBase = 0x70000000;
    std::array<std::string_view, 2> const kinds = { "Static", "Vector" };
    std::vector<uint64> containers;
    std::vector<uint64> names;
    for (std::size_t kind = 0; kind < kinds.size(); ++kind)
    {
        uint64 const vtable = h.heap.Allocate(16 * 8, true);
        for (uint64 k = 0; k < 16; ++k)
            h.machine.WriteU64(vtable + 8 * k, FunctionBase + kind * 0x100 + k * 8);
        containers.push_back(h.heap.Allocate(0x40, true));
        h.machine.WriteU64(containers.back(), vtable);
        std::vector<uint8> text(kinds[kind].begin(), kinds[kind].end());
        text.push_back(0);
        names.push_back(h.heap.Allocate(text.size(), true));
        h.machine.Write(names.back(), text);
    }
    GuestCall const call = [&](uint64 function, uint64) -> std::optional<uint64>
    {
        uint64 const kind = (function - FunctionBase) / 0x100;
        uint64 const entry = (function - FunctionBase) % 0x100 / 8;
        if (entry == shifted.ContainerNameSlot)
            return names[kind];
        if (entry == shifted.ContainerDynamicSlot)
            return kind == 0 ? 0 : 1;
        return std::nullopt;
    };
    uint64 const intType = h.objects.Type("int");
    std::vector<uint64> types = { intType };
    for (int i = 0; i < 24; ++i)
    {
        std::string const name = fmt::format("class C{}", i);
        uint64 const list = h.objects.List(TypeWalker::ListNameOf(name), 0);
        h.objects.SetProperties(list, { h.objects.Property(intType, "int", "m_first", 0, 0x48, 31, containers[0]),
            h.objects.Property(intType, "int", "m_second", 1, 0x4C, 31, containers[static_cast<std::size_t>(i % 2)]),
            h.objects.Property(intType, "int", "m_third", 2, 0x50, 31, containers[1]) });
        types.push_back(h.objects.Type(name, 0x140010000, false, list));
    }

    std::vector<ListedProperty> const properties = ClientDiscovery::ListedProperties(h.machine, h.heap, types, shifted);
    ASSERT_EQ(properties.size(), 72u);
    ClientLayout derived = shifted;
    derived.PropertyId = derived.PropertyContainer = derived.ContainerNameSlot = derived.ContainerDynamicSlot = 0x99;
    std::string error;
    ASSERT_TRUE(ClientDiscovery::DerivePropertyId(h.machine, properties, derived, error)) << error;
    ASSERT_TRUE(ClientDiscovery::DeriveContainerLayout(h.machine, properties, derived, call, error)) << error;
    EXPECT_EQ(derived.PropertyId, shifted.PropertyId);
    EXPECT_EQ(derived.PropertyContainer, shifted.PropertyContainer);
    EXPECT_EQ(derived.ContainerNameSlot, shifted.ContainerNameSlot);
    EXPECT_EQ(derived.ContainerDynamicSlot, shifted.ContainerDynamicSlot);

    GuestCall const allStatic = [&](uint64 function, uint64 object) -> std::optional<uint64>
    {
        return (function - FunctionBase) % 0x100 / 8 == shifted.ContainerNameSlot ? std::optional<uint64>(names[0]) : call(function, object);
    };
    ClientLayout refused = shifted;
    EXPECT_FALSE(ClientDiscovery::DeriveContainerLayout(h.machine, properties, refused, allStatic, error));
    EXPECT_NE(error.find("Property.container could not be placed"), std::string::npos) << error;
    EXPECT_FALSE(refused.DerivedFields.contains("Property.container"));
}
