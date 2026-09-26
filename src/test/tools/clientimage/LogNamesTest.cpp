/*
 * Project Ambrose by Imjustchico
 * Tests the names functions log under: what counts as a qualified name and as a source path, and over a PeBuilder image, that a function reading names right beside a source path logs under each in order, while one reading a name with no path and one reading it too far from the path log under nothing.
 */

#include "CodeBuffer.h"
#include "CodeIndex.h"
#include "LogNames.h"
#include "PeBuilder.h"
#include "PeImage.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;
    constexpr uint32 Text = 0x1000;
    constexpr uint32 Rdata = 0x2000;
    constexpr uint32 Path = Rdata + 0x10;
    constexpr uint32 Load = Rdata + 0x40;
    constexpr uint32 Save = Rdata + 0x60;
    constexpr uint32 Entry = Rdata + 0x80;
}

TEST(LogNamesTest, AQualifiedNameIsAScopeThenAnIdentifierDestructorOrOperator)
{
    for (std::string const name : { "CoreObjectFactory::AddBehavior", "A::B::C", "Outer<int>::Inner", "EventHandler<class ClientPetNameBehavior,class DMLRecord &>::operator ()",
             "Widget::~Widget", "Holder::Get<class Item *>", "Pets::operator==" })
        EXPECT_TRUE(LogNames::IsQualifiedName(name)) << name;
    for (std::string const text : { "CoreObjectFactory", "Foo::Bar - failed", "Foo::", "::Foo", "%p Foo::Bar", "Foo::Bar()", "C:\\Code\\A::B.cpp", "Foo:: Bar",
             "Foo<int::Bar", "Foo::operator%s" })
        EXPECT_FALSE(LogNames::IsQualifiedName(text)) << text;
}

TEST(LogNamesTest, ASourcePathEndsInASourceOrHeaderExtensionAfterAFolder)
{
    for (std::string const path : { "C:\\Code\\Wizard101\\Wizard_1_610\\Core\\Common\\CoreObjects\\CoreObject.cpp", "src/Thing.h", "lib/x.INL" })
        EXPECT_TRUE(LogNames::IsSourcePath(path)) << path;
    for (std::string const text : { "CoreObject.cpp", "C:\\Code\\readme.txt", "notes/x.cppx", "C:\\Code\\" })
        EXPECT_FALSE(LogNames::IsSourcePath(text)) << text;
}

TEST(LogNamesTest, AFunctionLogsUnderTheNamesItReadsBesideASourcePath)
{
    CodeBuffer code(Text, 0x100);
    code.Lea(Text, 0x48, 0x05, Path);
    code.Lea(Text + 0x07, 0x48, 0x05, Load);
    code.Lea(Text + 0x0E, 0x48, 0x05, Path);
    code.Lea(Text + 0x15, 0x48, 0x05, Save);
    code.Lea(Text + 0x1C, 0x48, 0x05, Load);
    code.Put(Text + 0x23, { 0xC3 });
    code.Lea(Text + 0x40, 0x48, 0x05, Entry);
    code.Put(Text + 0x47, { 0xC3 });
    code.Lea(Text + 0x80, 0x48, 0x05, Path);
    for (uint32 at = Text + 0x87; at < Text + 0xC0; ++at)
        code.Put(at, { 0x90 });
    code.Lea(Text + 0xC0, 0x48, 0x05, Entry);
    code.Put(Text + 0xC7, { 0xC3 });

    std::vector<uint8> rdata(0xA0, 0);
    auto const put = [&rdata](uint32 at, std::string const& text) { std::copy(text.begin(), text.end(), rdata.begin() + (at - Rdata)); };
    put(Path, "C:\\Code\\Sample\\Sample.cpp");
    put(Load, "Sample::Load");
    put(Save, "Sample::Save");
    put(Entry, "Registry::Entry");

    PeBuilder builder(Base);
    builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics);
    builder.AddSection(".rdata", rdata, PeBuilder::ReadOnlyCharacteristics);
    builder.AddFunction(Text, Text + 0x24);
    builder.AddFunction(Text + 0x40, Text + 0x48);
    builder.AddFunction(Text + 0x80, Text + 0xC8);
    std::string error;
    std::unique_ptr<PeImage> const image = PeImage::Parse(builder.Build(), error);
    ASSERT_NE(image, nullptr) << error;
    CodeIndex const index(*image);

    std::vector<FunctionLogNames> const named = LogNames::Find(*image, index);
    ASSERT_EQ(named.size(), 1u) << "a name read with no path beside it, or too far from one, is not the function's own";
    EXPECT_EQ(named.front().Function, Base + Text);
    EXPECT_EQ(named.front().Names, (std::vector<std::string>{ "Sample::Load", "Sample::Save" })) << "each name once, in the order the code reads them";
}
