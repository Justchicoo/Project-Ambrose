/*
 * Project Ambrose by Imjustchico
 * Tests the type walker over synthetic engine objects: classes with bases, properties with containers named by their own methods, pointer and singleton flags, enum options as numbers wrapped to 32 bits or text, types and options the client registered twice kept once unless the copies disagree, validation problems for bad hashes, ids and containers and for names or option text that are not valid UTF-8, and the property list names of wrapped types.
 */

#include "GuestObjects.h"
#include "GuestProcess.h"
#include "LogTestDirectory.h"
#include "TypeWalker.h"

#include <gtest/gtest.h>

#include <variant>

namespace
{
    constexpr uint64 CodeBase = 0x50000000;

    struct World
    {
        LogTestDirectory directory;
        std::unique_ptr<GuestProcess> process;
        std::unique_ptr<GuestObjects> objects;
        uint64 staticContainer = 0;
        uint64 vectorContainer = 0;
        uint64 mapContainer = 0;

        World()
        {
            GuestProcess::Options options;
            options.Folder = directory.Path();
            options.HeapSize = 0x1000000;
            process = std::make_unique<GuestProcess>(options);
            Machine& machine = process->GetMachine();
            objects = std::make_unique<GuestObjects>(machine, process->GetHeap());
            machine.Map(CodeBase, 0x1000);
            auto nameFunction = [&](uint64 at, uint64 text, std::string_view name)
            {
                int32 const disp = static_cast<int32>(static_cast<int64>(CodeBase + text) - static_cast<int64>(CodeBase + at + 7));
                std::vector<uint8> code = { 0x48, 0x8D, 0x05, 0, 0, 0, 0, 0xC3 };
                for (int i = 0; i < 4; ++i)
                    code[3 + static_cast<std::size_t>(i)] = static_cast<uint8>(static_cast<uint32>(disp) >> (8 * i));
                machine.Write(CodeBase + at, code);
                std::vector<uint8> bytes(name.begin(), name.end());
                bytes.push_back(0);
                machine.Write(CodeBase + text, bytes);
            };
            nameFunction(0x00, 0x100, "Static");
            nameFunction(0x10, 0x110, "Vector");
            nameFunction(0x20, 0x120, "Map");
            machine.Write(CodeBase + 0x30, std::vector<uint8>{ 0x31, 0xC0, 0xC3 });
            machine.Write(CodeBase + 0x40, std::vector<uint8>{ 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 });
            auto container = [&](uint64 name, uint64 dynamic)
            {
                uint64 const vtable = process->GetHeap().Allocate(0x40, true);
                machine.WriteU64(vtable + 8, CodeBase + name);
                machine.WriteU64(vtable + 32, CodeBase + dynamic);
                uint64 const object = process->GetHeap().Allocate(0x40, true);
                machine.WriteU64(object, vtable);
                return object;
            };
            staticContainer = container(0x00, 0x30);
            vectorContainer = container(0x10, 0x40);
            mapContainer = container(0x20, 0x40);
        }
    };
}

TEST(TypeWalkerTest, WalksClassesPropertiesContainersAndOptions)
{
    World world;
    GuestObjects& o = *world.objects;
    uint64 const baseList = o.List("PropertyClass", 0);
    uint64 const list = o.List("Widget", baseList, true);
    uint64 const widget = o.Type("class Widget", 0x140010000, false, list);
    uint64 const integer = o.Type("int");
    uint64 const shared = o.Type("class SharedPointer<class Widget>", 0x140010000, true, list);
    uint64 const count = o.Property(integer, "int", "m_count", 0, 0x48, 31, world.staticContainer);
    uint64 const items = o.Property(shared, "class SharedPointer<class Widget>", "m_items", 1, 0x50, 7, world.vectorContainer);
    uint64 const mode = o.Property(integer, "int", "m_mode", 2, 0x68, 2097183, world.staticContainer);
    o.SetOptions(mode, { { "kOne", "1" }, { "kNeg", "-1" }, { "kHuge", "4294967297" }, { "", "" }, { "__DEFAULT", "kOne" } });
    o.SetProperties(list, { count, items, mode });

    TypeWalker walker(*world.process, {});
    std::vector<uint64> const types = { widget, integer, shared };
    TypeWalkResult const result = walker.Walk(types);
    ASSERT_TRUE(result.IsValid()) << result.ProblemCounts.begin()->first << ": " << result.ProblemSamples.begin()->second.front();
    ASSERT_EQ(result.Dump.Classes.size(), 3u);
    EXPECT_EQ(result.PropertyCount, 6u);

    TypeDumpLoader::RawClass const& widgetClass = result.Dump.Classes[0];
    EXPECT_EQ(widgetClass.Key, std::to_string(StringHash::KiStringHash("class Widget")));
    EXPECT_EQ(widgetClass.Name, "class Widget");
    EXPECT_EQ(widgetClass.Bases, std::vector<std::string>{ "PropertyClass" });
    ASSERT_EQ(widgetClass.Properties.size(), 3u);
    TypeDumpLoader::RawProperty const& first = widgetClass.Properties[0];
    EXPECT_EQ(first.Name, "m_count");
    EXPECT_EQ(first.Type, "int");
    EXPECT_EQ(first.Container, "Static");
    EXPECT_EQ(first.Id, 0u);
    EXPECT_EQ(first.Offset, 0x48u);
    EXPECT_EQ(first.Flags, 31u);
    EXPECT_EQ(first.Hash, StringHash::PropertyHash("int", "m_count"));
    EXPECT_EQ(first.Dynamic, false);
    EXPECT_EQ(first.Singleton, true);
    EXPECT_EQ(first.Pointer, false);
    EXPECT_TRUE(first.Options.empty());
    TypeDumpLoader::RawProperty const& second = widgetClass.Properties[1];
    EXPECT_EQ(second.Container, "Vector");
    EXPECT_EQ(second.Dynamic, true);
    EXPECT_EQ(second.Pointer, true);
    TypeDumpLoader::RawProperty const& third = widgetClass.Properties[2];
    ASSERT_EQ(third.Options.size(), 5u);
    EXPECT_EQ(third.Options[0].first, "kOne");
    EXPECT_EQ(std::get<int64>(third.Options[0].second), 1);
    EXPECT_EQ(std::get<int64>(third.Options[1].second), 4294967295);
    EXPECT_EQ(std::get<int64>(third.Options[2].second), 1);
    EXPECT_EQ(std::get<std::string>(third.Options[3].second), "");
    EXPECT_EQ(std::get<std::string>(third.Options[4].second), "kOne");

    EXPECT_TRUE(result.Dump.Classes[1].Properties.empty());
    EXPECT_TRUE(result.Dump.Classes[1].Bases.empty());
    EXPECT_EQ(result.Dump.Classes[2].Properties.size(), 3u);
}

TEST(TypeWalkerTest, ValidationCountsEveryProblemKind)
{
    World world;
    GuestObjects& o = *world.objects;
    uint64 const list = o.List("Wrong", 0);
    uint64 const widget = o.Type("class Widget", 0x140010000, false, list);
    uint64 const integer = o.Type("int");
    uint64 const hashed = o.Property(integer, "int", "m_hash", 0, 0x48, 31, world.staticContainer);
    world.process->GetMachine().WriteU32(hashed + ClientLayout{}.PropertyHash, 1);
    uint64 const misplaced = o.Property(integer, "int", "m_id", 7, 0x4C, 31, world.staticContainer);
    uint64 const mapped = o.Property(integer, "int", "m_map", 2, 0x50, 31, world.mapContainer);
    o.SetProperties(list, { hashed, misplaced, mapped });
    uint64 const badType = o.Type("class Bad");
    world.process->GetMachine().WriteU32(badType + ClientLayout{}.TypeHash, 99);
    uint64 const unreadable = world.process->GetHeap().Allocate(0xB0, true);
    world.process->GetMachine().WriteU64(unreadable + ClientLayout{}.TypeName + ClientLayout{}.StringCapacity, 64);
    world.process->GetMachine().WriteU64(unreadable + ClientLayout{}.TypeName + ClientLayout{}.StringSize, 32);
    world.process->GetMachine().WriteU64(unreadable + ClientLayout{}.TypeName, 0xDEAD000000);

    TypeWalker walker(*world.process, {});
    std::vector<uint64> const types = { widget, badType, unreadable };
    TypeWalkResult const result = walker.Walk(types);
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.ProblemCounts.at("property list name"), 1u);
    EXPECT_EQ(result.ProblemCounts.at("property hash"), 1u);
    EXPECT_EQ(result.ProblemCounts.at("property id"), 1u);
    EXPECT_EQ(result.ProblemCounts.at("container"), 1u);
    EXPECT_EQ(result.ProblemCounts.at("type hash"), 1u);
    EXPECT_EQ(result.ProblemCounts.at("unreadable type"), 1u);
    EXPECT_NE(result.ProblemSamples.at("property id").front().find("m_id"), std::string::npos);
}

TEST(TypeWalkerTest, NamesAndOptionTextThatAreNotUtf8AreProblems)
{
    World world;
    GuestObjects& o = *world.objects;
    std::string const className = std::string("class Bad") + "\xFF" + "Name";
    std::string const baseName = std::string("Base") + "\xC3";
    std::string const typeName = std::string("enum Mood") + "\xE2\x82";
    std::string const propertyName = std::string("m_") + "\xF0";
    std::string const optionName = std::string("k") + "\xC3";
    std::string const optionText = "\xED\xA0\x80";
    std::string const fineText = std::string("caf") + "\xC3\xA9";
    std::string const listName = TypeWalker::ListNameOf(className);
    uint64 const baseList = o.List(baseName, 0);
    uint64 const list = o.List(listName, baseList);
    uint64 const mood = o.Type(typeName);
    uint64 const property = o.Property(mood, typeName, propertyName, 0, 0x48, 31, world.staticContainer);
    o.SetOptions(property, { { optionName, "1" }, { "kText", optionText }, { "kFine", fineText } });
    o.SetProperties(list, { property });
    uint64 const bad = o.Type(className, 0x140010000, false, list);
    uint64 const integer = o.Type("int");
    std::string const goodName = std::string("m_caf") + "\xC3\xA9";
    std::string const goodOption = std::string("k") + "\xE2\x82\xAC";
    std::string const goodText = "\xF0\x9F\x98\x80";
    uint64 const goodList = o.List("Good", 0);
    uint64 const good = o.Property(integer, "int", goodName, 0, 0x48, 31, world.staticContainer);
    o.SetOptions(good, { { goodOption, goodText } });
    o.SetProperties(goodList, { good });
    uint64 const goodType = o.Type("class Good", 0x140010000, false, goodList);

    auto const describe = [](TypeWalkResult const& walked)
    {
        std::string text;
        for (auto const& [kind, samples] : walked.ProblemSamples)
            for (std::string const& sample : samples)
                text += kind + ": " + sample + "\n";
        return text;
    };
    TypeWalker walker(*world.process, {});
    std::vector<uint64> const clean = { integer, goodType };
    TypeWalkResult const valid = walker.Walk(clean);
    EXPECT_TRUE(valid.IsValid()) << describe(valid);

    std::vector<uint64> const types = { bad, integer, goodType };
    TypeWalkResult const result = walker.Walk(types);
    EXPECT_FALSE(result.IsValid());
    ASSERT_EQ(result.ProblemCounts.size(), 1u) << describe(result);
    ASSERT_EQ(result.ProblemCounts.count("invalid text"), 1u) << describe(result);
    EXPECT_EQ(result.ProblemCounts.at("invalid text"), 6u);
    std::vector<std::string> const& samples = result.ProblemSamples.at("invalid text");
    ASSERT_EQ(samples.size(), TypeWalkResult::SamplesPerProblem);
    EXPECT_EQ(samples[0], "the class name class Bad\\xFFName is not valid UTF-8");
    EXPECT_EQ(samples[1], "class Bad\\xFFName names the base Base\\xC3, which is not valid UTF-8");
    EXPECT_EQ(samples[2], "class Bad\\xFFName has the property name m_\\xF0, which is not valid UTF-8");
    EXPECT_EQ(samples[3], "class Bad\\xFFName.m_\\xF0 has the type enum Mood\\xE2\\x82, which is not valid UTF-8");
    EXPECT_EQ(samples[4], "class Bad\\xFFName.m_\\xF0 has the option name k\\xC3, which is not valid UTF-8");
    ASSERT_EQ(result.Dump.Classes.size(), 3u);
    EXPECT_EQ(result.Dump.Classes[0].Name, className);
    EXPECT_EQ(result.Dump.Classes[0].Bases, std::vector<std::string>{ baseName });
    ASSERT_EQ(result.Dump.Classes[0].Properties.size(), 1u);
    EXPECT_EQ(result.Dump.Classes[0].Properties[0].Options.size(), 3u);
}

TEST(TypeWalkerTest, TypesAndOptionsRegisteredTwiceAreKeptOnceUnlessTheyDisagree)
{
    World world;
    GuestObjects& o = *world.objects;
    uint64 const integer = o.Type("int");
    uint64 const list = o.List("Widget", 0);
    uint64 const mode = o.Property(integer, "int", "m_mode", 0, 0x48, 31, world.staticContainer);
    o.SetOptions(mode, { { "kNone", "0" }, { "kOne", "1" }, { "kNone", "0" } });
    o.SetProperties(list, { mode });
    uint64 const first = o.Type("class Widget", 0x140010000, false, list);
    uint64 const second = o.Type("class Widget", 0x140020000, false, list);

    TypeWalker walker(*world.process, {});
    std::vector<uint64> const types = { integer, first, second };
    TypeWalkResult const result = walker.Walk(types);
    ASSERT_TRUE(result.IsValid()) << result.ProblemCounts.begin()->first;
    EXPECT_EQ(result.Dump.Classes.size(), 2u);
    EXPECT_EQ(result.DuplicateTypes, 1u);
    EXPECT_EQ(result.DuplicateOptions, 2u);
    EXPECT_EQ(result.PropertyCount, 1u);
    ASSERT_EQ(result.Dump.Classes[1].Properties.size(), 1u);
    EXPECT_EQ(result.Dump.Classes[1].Properties[0].Options.size(), 2u);

    uint64 const otherList = o.List("Widget", 0);
    uint64 const other = o.Property(integer, "int", "m_other", 0, 0x48, 31, world.staticContainer);
    o.SetProperties(otherList, { other });
    uint64 const conflicting = o.Type("class Widget", 0x140030000, false, otherList);
    world.process->GetMachine().WriteU64(mode + ClientLayout{}.PropertyOptions + 8, world.process->GetMachine().ReadU64(mode + ClientLayout{}.PropertyOptions) + 2 * ClientLayout{}.OptionSize);
    uint64 const clash = o.Property(integer, "int", "m_clash", 0, 0x48, 31, world.staticContainer);
    o.SetOptions(clash, { { "kNone", "0" }, { "kNone", "1" } });
    uint64 const clashList = o.List("Clash", 0);
    o.SetProperties(clashList, { clash });
    uint64 const clashType = o.Type("class Clash", 0x140010000, false, clashList);
    std::vector<uint64> const conflicts = { first, conflicting, clashType };
    TypeWalkResult const bad = walker.Walk(conflicts);
    EXPECT_EQ(bad.ProblemCounts.at("conflicting duplicate type"), 1u);
    EXPECT_EQ(bad.ProblemCounts.at("conflicting enum option"), 1u);
    EXPECT_EQ(bad.Dump.Classes.size(), 2u);
}

TEST(TypeWalkerTest, PropertyListNamesUnwrapPointersAndStandardStrings)
{
    EXPECT_EQ(TypeWalker::ListNameOf("class Duel"), "Duel");
    EXPECT_EQ(TypeWalker::ListNameOf("class Duel*"), "Duel");
    EXPECT_EQ(TypeWalker::ListNameOf("class SharedPointer<class Duel>"), "Duel");
    EXPECT_EQ(TypeWalker::ListNameOf("struct GoalEventData"), "GoalEventData");
    EXPECT_EQ(TypeWalker::ListNameOf("WeightedEntryT<class SplashCinematicInfo>"), "WeightedEntryT<SplashCinematicInfo>");
    EXPECT_EQ(TypeWalker::ListNameOf("class MadlibArgT<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >*"), "MadlibArgT<std::string>");
    EXPECT_EQ(TypeWalker::ListNameOf("class MadlibArgT<class std::basic_string<wchar_t,struct std::char_traits<wchar_t>,class std::allocator<wchar_t> > >*"), "MadlibArgT<std::wstring>");
    EXPECT_EQ(TypeWalker::ListNameOf("class MadlibArgT<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const >*"), "MadlibArgT<std::string const>");
    EXPECT_EQ(TypeWalker::ListNameOf("class Subclass"), "Subclass");
}
