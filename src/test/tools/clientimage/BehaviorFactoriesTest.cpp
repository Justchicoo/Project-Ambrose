/*
 * Project Ambrose by Imjustchico
 * Tests the behavior factory finder over a hand-assembled PeBuilder image laid out the way the client registers its behaviors: one name turned into its id and its factory stored in place, one whose factory is stored inside the registration function called next, a create function that writes the object's vtable itself and one whose constructor writes it, a GetType the exception table splits into two regions, one reached through a jump thunk that registers its class under the type its parent's GetType returns, a parent type overwritten through a 32-bit register, names that follow a stray printable byte, a name read in a log call that registers nothing, and a name followed by a vtable whose second slot makes no object; and which names read as behavior names.
 */

#include "BehaviorFactories.h"
#include "CodeBuffer.h"
#include "CodeIndex.h"
#include "PeBuilder.h"
#include "PeImage.h"

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;
    constexpr uint32 Text = 0x1000;
    constexpr uint32 Rdata = 0x2000;

    constexpr uint32 Registrar = Text + 0x00;
    constexpr uint32 RegisterOther = Text + 0x60;
    constexpr uint32 CreateTest = Text + 0x80;
    constexpr uint32 CreateOther = Text + 0xA0;
    constexpr uint32 ConstructOther = Text + 0xB0;
    constexpr uint32 GetTypeTest = Text + 0xD0;
    constexpr uint32 ThunkOther = Text + 0x100;
    constexpr uint32 GetTypeOther = Text + 0x110;
    constexpr uint32 NotCreate = Text + 0x128;
    constexpr uint32 Hash = Text + 0x138;
    constexpr uint32 Allocate = Text + 0x140;
    constexpr uint32 ConstructBase = Text + 0x148;
    constexpr uint32 RegisterType = Text + 0x150;
    constexpr uint32 Log = Text + 0x158;
    constexpr uint32 Destroy = Text + 0x160;

    constexpr uint32 NameTest = Rdata + 0x00;
    constexpr uint32 NameOther = Rdata + 0x20;
    constexpr uint32 NameLogged = Rdata + 0x40;
    constexpr uint32 ClassTest = Rdata + 0x60;
    constexpr uint32 ClassOther = Rdata + 0x70;
    constexpr uint32 FactoryTest = Rdata + 0x80;
    constexpr uint32 FactoryOther = Rdata + 0x90;
    constexpr uint32 ObjectTest = Rdata + 0xA0;
    constexpr uint32 ObjectOther = Rdata + 0xB0;
    constexpr uint32 NameFalse = Rdata + 0xC0;
    constexpr uint32 FactoryFalse = Rdata + 0xD0;

    void Place(std::vector<uint8>& data, uint32 rva, std::string_view text)
    {
        std::memcpy(data.data() + (rva - Rdata), text.data(), text.size());
    }

    void Pointer(std::vector<uint8>& data, uint32 rva, uint32 target)
    {
        uint64 const value = Base + target;
        for (std::size_t at = 0; at < 8; ++at)
            data[rva - Rdata + at] = static_cast<uint8>(value >> (8 * at));
    }

    std::vector<uint8> RegistrationImage()
    {
        CodeBuffer code(Text, 0x200);
        code.Lea(Registrar + 0x00, 0x48, 0x0D, NameTest);
        code.Call(Registrar + 0x07, Hash);
        code.Call(Registrar + 0x0C, Allocate);
        code.Put(Registrar + 0x11, { 0x48, 0x89, 0xC3 });
        code.Lea(Registrar + 0x14, 0x48, 0x05, FactoryTest);
        code.Put(Registrar + 0x1B, { 0x48, 0x89, 0x03 });
        code.Lea(Registrar + 0x1E, 0x48, 0x0D, NameOther);
        code.Call(Registrar + 0x25, Hash);
        code.Call(Registrar + 0x2A, RegisterOther);
        code.Lea(Registrar + 0x2F, 0x48, 0x0D, NameLogged);
        code.Call(Registrar + 0x36, Log);
        code.Lea(Registrar + 0x3B, 0x48, 0x0D, NameFalse);
        code.Call(Registrar + 0x42, Hash);
        code.Lea(Registrar + 0x47, 0x48, 0x05, FactoryFalse);
        code.Put(Registrar + 0x4E, { 0x48, 0x89, 0x03 });
        code.Put(Registrar + 0x51, { 0xC3 });

        code.Call(RegisterOther + 0x00, Allocate);
        code.Put(RegisterOther + 0x05, { 0x48, 0x89, 0xC3 });
        code.Lea(RegisterOther + 0x08, 0x48, 0x05, FactoryOther);
        code.Put(RegisterOther + 0x0F, { 0x48, 0x89, 0x03 });
        code.Put(RegisterOther + 0x12, { 0xC3 });

        code.Call(CreateTest + 0x00, Allocate);
        code.Put(CreateTest + 0x05, { 0x48, 0x89, 0xC7 });
        code.Put(CreateTest + 0x08, { 0x48, 0x89, 0xC1 });
        code.Call(CreateTest + 0x0B, ConstructBase);
        code.Lea(CreateTest + 0x10, 0x48, 0x05, ObjectTest);
        code.Put(CreateTest + 0x17, { 0x48, 0x89, 0x07 });
        code.Put(CreateTest + 0x1A, { 0x48, 0x89, 0xF8 });
        code.Put(CreateTest + 0x1D, { 0xC3 });

        code.Call(CreateOther + 0x00, Allocate);
        code.Put(CreateOther + 0x05, { 0x48, 0x89, 0xC1 });
        code.Call(CreateOther + 0x08, ConstructOther);
        code.Put(CreateOther + 0x0D, { 0xC3 });

        code.Put(ConstructOther + 0x00, { 0x48, 0x89, 0xCB });
        code.Call(ConstructOther + 0x03, ConstructBase);
        code.Lea(ConstructOther + 0x08, 0x48, 0x05, ObjectOther);
        code.Put(ConstructOther + 0x0F, { 0x48, 0x89, 0x03 });
        code.Put(ConstructOther + 0x12, { 0xC3 });

        code.Put(GetTypeTest + 0x00, { 0x48, 0x83, 0xEC, 0x28 });
        code.Call(GetTypeTest + 0x04, GetTypeOther);
        code.Put(GetTypeTest + 0x09, { 0x48, 0x89, 0xC2 });
        code.Put(GetTypeTest + 0x0C, { 0xBA, 0x00, 0x00, 0x00, 0x00 });
        code.Lea(GetTypeTest + 0x11, 0x48, 0x0D, ClassTest);
        code.Call(GetTypeTest + 0x18, RegisterType);
        code.Put(GetTypeTest + 0x1D, { 0x48, 0x83, 0xC4, 0x28 });
        code.Put(GetTypeTest + 0x21, { 0xC3 });

        code.Put(ThunkOther, { 0xE9 });
        code.Displacement(ThunkOther + 0x01, ThunkOther + 0x05, GetTypeOther);

        code.Call(GetTypeOther + 0x00, GetTypeTest);
        code.Put(GetTypeOther + 0x05, { 0x48, 0x89, 0xC2 });
        code.Lea(GetTypeOther + 0x08, 0x48, 0x0D, ClassOther);
        code.Call(GetTypeOther + 0x0F, RegisterType);
        code.Put(GetTypeOther + 0x14, { 0xC3 });

        code.Lea(NotCreate + 0x00, 0x48, 0x05, FactoryFalse);
        code.Put(NotCreate + 0x07, { 0x48, 0x89, 0x01 });
        code.Put(NotCreate + 0x0A, { 0xC3 });

        for (uint32 const stub : { Hash, Allocate, ConstructBase, RegisterType, Log, Destroy })
            code.Put(stub, { 0xC3 });

        std::vector<uint8> rdata(0xE0, 0);
        Place(rdata, NameTest, "TestBehavior");
        Place(rdata, NameOther - 1, "QOtherBehavior");
        Place(rdata, NameLogged, "LoggedBehavior");
        Place(rdata, ClassTest, "ClassNameA");
        Place(rdata, ClassOther - 2, "\x02" "CClassNameB");
        Place(rdata, NameFalse, "FalseBehavior");
        Pointer(rdata, FactoryTest, Destroy);
        Pointer(rdata, FactoryTest + 8, CreateTest);
        Pointer(rdata, FactoryOther, Destroy);
        Pointer(rdata, FactoryOther + 8, CreateOther);
        Pointer(rdata, ObjectTest, GetTypeTest);
        Pointer(rdata, ObjectTest + 8, Destroy);
        Pointer(rdata, ObjectOther, ThunkOther);
        Pointer(rdata, ObjectOther + 8, Destroy);
        Pointer(rdata, FactoryFalse, Destroy);
        Pointer(rdata, FactoryFalse + 8, NotCreate);

        PeBuilder builder(Base);
        EXPECT_EQ(builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics), Text);
        EXPECT_EQ(builder.AddSection(".rdata", rdata, PeBuilder::ReadOnlyCharacteristics), Rdata);
        builder.AddFunction(Registrar, Registrar + 0x52);
        builder.AddFunction(RegisterOther, RegisterOther + 0x13);
        builder.AddFunction(CreateTest, CreateTest + 0x1E);
        builder.AddFunction(CreateOther, CreateOther + 0x0E);
        builder.AddFunction(ConstructOther, ConstructOther + 0x13);
        builder.AddFunction(GetTypeTest, GetTypeTest + 0x04);
        builder.AddChainedFunction(GetTypeTest + 0x04, GetTypeTest + 0x22, GetTypeTest, GetTypeTest + 0x04);
        builder.AddFunction(GetTypeOther, GetTypeOther + 0x15);
        builder.AddFunction(NotCreate, NotCreate + 0x0B);
        return builder.Build();
    }
}

TEST(BehaviorFactoriesTest, EachRegisteredBehaviorIsTheClassItsFactoryBuilds)
{
    std::string error;
    std::unique_ptr<PeImage> const image = PeImage::Parse(RegistrationImage(), error);
    ASSERT_NE(image, nullptr) << error;
    CodeIndex const index(*image);

    std::vector<BehaviorFactory> const found = BehaviorFactories::Find(*image, index);
    ASSERT_EQ(found.size(), 2u) << "a behavior name only logged registers no factory, nor does one whose factory makes no object";

    EXPECT_EQ(found[0].Behavior, "OtherBehavior") << "a name is the text at the address the code loads, even right after a printable byte";
    EXPECT_EQ(found[0].Site, Base + Registrar + 0x1E);
    EXPECT_EQ(found[0].FactoryVtable, Base + FactoryOther) << "a factory stored inside the registration function called after the name is found there";
    EXPECT_EQ(found[0].Create, Base + CreateOther);
    EXPECT_EQ(found[0].ObjectVtable, Base + ObjectOther) << "a create function that writes no vtable is followed into the constructor it calls";
    EXPECT_EQ(found[0].GetType, Base + GetTypeOther) << "a jump thunk in the vtable is followed to GetType";
    EXPECT_EQ(found[0].ClassName, "ClassNameB") << "so is a class name";
    EXPECT_EQ(found[0].Bases, std::vector<std::string>{ "ClassNameA" }) << "the type a parent's GetType returns and GetType registers its class under is followed to that parent's name";

    EXPECT_EQ(found[1].Behavior, "TestBehavior");
    EXPECT_EQ(found[1].Site, Base + Registrar);
    EXPECT_EQ(found[1].FactoryVtable, Base + FactoryTest) << "a factory stored in place after the name's id is taken is found, past the allocation called first";
    EXPECT_EQ(found[1].Create, Base + CreateTest);
    EXPECT_EQ(found[1].ObjectVtable, Base + ObjectTest) << "the object's pointer is followed from the allocation into the register the vtable is written through";
    EXPECT_EQ(found[1].GetType, Base + GetTypeTest);
    EXPECT_EQ(found[1].ClassName, "ClassNameA") << "a GetType the exception table splits is read past its first region";
    EXPECT_TRUE(found[1].Bases.empty()) << "a 32-bit write to edx replaces the parent type rdx held, so the class registers with none";
}

TEST(BehaviorFactoriesTest, ABehaviorNameIsOneCapitalizedWordEndingInBehavior)
{
    EXPECT_TRUE(BehaviorFactories::IsBehaviorName("NPCBehavior"));
    EXPECT_TRUE(BehaviorFactories::IsBehaviorName("BasicSpellbookBehavior"));
    EXPECT_FALSE(BehaviorFactories::IsBehaviorName("Behavior"));
    EXPECT_FALSE(BehaviorFactories::IsBehaviorName("npcBehavior"));
    EXPECT_FALSE(BehaviorFactories::IsBehaviorName("NPC Behavior"));
    EXPECT_FALSE(BehaviorFactories::IsBehaviorName("NPCBehaviorTemplate"));
    EXPECT_FALSE(BehaviorFactories::IsBehaviorName("class NPCBehavior"));
}
