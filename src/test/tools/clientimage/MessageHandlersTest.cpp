/*
 * Project Ambrose by Imjustchico
 * Tests the message handler finder over a hand-assembled PeBuilder image laid out the way the client registers its handlers: a function that loads each handler's address, reads its plain MSG_Name, once as a string of its own and once as the tail of the debug name, and then loads its Class::MSG_Name, a registration that copies both names with mov and movups rather than loading them, two registrations in a row that each pass their handler to the registering call after the name, a second reference to one debug name in the same registration, a handler that reads its own plain and debug names as it posts itself for later, a registration that loads no function address, a function that only names itself in a log call, and a plain message name and an unreferenced debug name that register nothing; and which strings read as Class::MSG_Name at all.
 */

#include "CodeBuffer.h"
#include "CodeIndex.h"
#include "MessageHandlers.h"
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
    constexpr uint32 Registrar = Text;
    constexpr uint32 HandlerA = Text + 0x40;
    constexpr uint32 HandlerB = Text + 0x50;
    constexpr uint32 Lonely = Text + 0x60;
    constexpr uint32 Logger = Text + 0x80;
    constexpr uint32 Mover = Text + 0xA0;
    constexpr uint32 HandlerC = Text + 0xD0;
    constexpr uint32 Poster = Text + 0x100;
    constexpr uint32 Register = Text + 0x140;
    constexpr uint32 HandlerX = Text + 0x150;
    constexpr uint32 HandlerY = Text + 0x160;
    constexpr uint32 DebugA = Rdata + 0x10;
    constexpr uint32 DebugB = Rdata + 0x30;
    constexpr uint32 DebugC = Rdata + 0x50;
    constexpr uint32 PlainPing = Rdata + 0x70;
    constexpr uint32 Unused = Rdata + 0x80;
    constexpr uint32 PlainNoHandler = Rdata + 0xA0;
    constexpr uint32 DebugD = Rdata + 0xC0;
    constexpr uint32 PlainMove = Rdata + 0xE0;
    constexpr uint32 DebugX = Rdata + 0x100;
    constexpr uint32 PlainX = Rdata + 0x120;
    constexpr uint32 DebugY = Rdata + 0x130;
    constexpr uint32 PlainY = Rdata + 0x150;

    void Place(std::vector<uint8>& data, uint32 rva, std::string_view text)
    {
        std::memcpy(data.data() + (rva - Rdata), text.data(), text.size());
    }

    std::vector<uint8> RegistrationImage()
    {
        CodeBuffer code(Text, 0x200);
        code.Lea(Registrar + 0x00, 0x48, 0x05, HandlerA);
        code.Lea(Registrar + 0x07, 0x48, 0x15, PlainPing);
        code.Lea(Registrar + 0x0E, 0x48, 0x0D, DebugA);
        code.Lea(Registrar + 0x15, 0x48, 0x15, DebugA);
        code.Lea(Registrar + 0x1C, 0x48, 0x05, HandlerB);
        code.Lea(Registrar + 0x23, 0x48, 0x15, DebugB + 16);
        code.Lea(Registrar + 0x2A, 0x48, 0x0D, DebugB);
        code.Put(Registrar + 0x31, { 0xC3 });
        code.Lea(HandlerA + 0x00, 0x48, 0x15, PlainPing);
        code.Lea(HandlerA + 0x07, 0x48, 0x0D, DebugA);
        code.Put(HandlerA + 0x0E, { 0xC3 });
        code.Put(HandlerB, { 0xC3 });
        code.Lea(Lonely + 0x00, 0x48, 0x15, PlainNoHandler);
        code.Lea(Lonely + 0x07, 0x48, 0x0D, DebugC);
        code.Put(Lonely + 0x0E, { 0xC3 });
        code.Lea(Logger + 0x00, 0x48, 0x05, HandlerB);
        code.Lea(Logger + 0x07, 0x48, 0x0D, DebugA);
        code.Put(Logger + 0x0E, { 0xC3 });
        code.Lea(Mover + 0x00, 0x48, 0x05, HandlerC);
        code.Put(Mover + 0x07, { 0x48, 0x8B, 0x0D });
        code.Displacement(Mover + 0x0A, Mover + 0x0E, PlainMove);
        code.Put(Mover + 0x0E, { 0x48, 0x8B, 0x15 });
        code.Displacement(Mover + 0x11, Mover + 0x15, PlainMove + 8);
        code.Put(Mover + 0x15, { 0x0F, 0x10, 0x05 });
        code.Displacement(Mover + 0x18, Mover + 0x1C, DebugD);
        code.Put(Mover + 0x1C, { 0x0F, 0x10, 0x0D });
        code.Displacement(Mover + 0x1F, Mover + 0x23, DebugD + 16);
        code.Put(Mover + 0x23, { 0xC3 });
        code.Put(HandlerC, { 0xC3 });
        code.Lea(Poster + 0x00, 0x48, 0x15, PlainX);
        code.Lea(Poster + 0x07, 0x48, 0x0D, DebugX);
        code.Lea(Poster + 0x0E, 0x4C, 0x0D, HandlerX);
        code.Call(Poster + 0x15, Register);
        code.Lea(Poster + 0x1A, 0x48, 0x15, PlainY);
        code.Lea(Poster + 0x21, 0x48, 0x0D, DebugY);
        code.Lea(Poster + 0x28, 0x4C, 0x0D, HandlerY);
        code.Call(Poster + 0x2F, Register);
        code.Put(Poster + 0x34, { 0xC3 });
        code.Put(Register, { 0xC3 });
        code.Put(HandlerX, { 0xC3 });
        code.Put(HandlerY, { 0xC3 });

        std::vector<uint8> rdata(0x180, 0);
        Place(rdata, DebugA, "Window::MSG_Ping");
        Place(rdata, DebugB, "ns::Other<int>::MSG_Pong");
        Place(rdata, DebugC, "Loose::MSG_NoHandler");
        Place(rdata, PlainPing, "MSG_Ping");
        Place(rdata, Unused, "Unused::MSG_Never");
        Place(rdata, PlainNoHandler, "MSG_NoHandler");
        Place(rdata, DebugD, "Mover::MSG_ByMoveLongName");
        Place(rdata, PlainMove, "MSG_ByMoveLongName");
        Place(rdata, DebugX, "Poster::MSG_PostX");
        Place(rdata, PlainX, "MSG_PostX");
        Place(rdata, DebugY, "Poster::MSG_PostY");
        Place(rdata, PlainY, "MSG_PostY");

        PeBuilder builder(Base);
        EXPECT_EQ(builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics), Text);
        EXPECT_EQ(builder.AddSection(".rdata", rdata, PeBuilder::ReadOnlyCharacteristics), Rdata);
        builder.AddFunction(Registrar, Registrar + 0x32);
        builder.AddFunction(HandlerA, HandlerA + 0x0F);
        builder.AddFunction(HandlerB, HandlerB + 1);
        builder.AddFunction(Lonely, Lonely + 0x0F);
        builder.AddFunction(Logger, Logger + 0x0F);
        builder.AddFunction(Mover, Mover + 0x24);
        builder.AddFunction(HandlerC, HandlerC + 1);
        builder.AddFunction(Poster, Poster + 0x35);
        builder.AddFunction(Register, Register + 1);
        builder.AddFunction(HandlerX, HandlerX + 1);
        builder.AddFunction(HandlerY, HandlerY + 1);
        return builder.Build();
    }
}

TEST(MessageHandlersTest, ARegistrationReadsThePlainNameAndLoadsTheHandlerBeforeTheDebugName)
{
    std::string error;
    std::unique_ptr<PeImage> const image = PeImage::Parse(RegistrationImage(), error);
    ASSERT_NE(image, nullptr) << error;
    CodeIndex const index(*image);

    std::vector<MessageHandlerRegistration> const found = MessageHandlers::Find(*image, index);
    ASSERT_EQ(found.size(), 6u) << "the log call naming its own function, the plain name alone and the debug name nothing refers to register nothing";

    EXPECT_EQ(found[0].Owner, "Mover");
    EXPECT_EQ(found[0].Handler, "MSG_ByMoveLongName");
    EXPECT_EQ(found[0].Site, Base + Mover + 0x15) << "a name copied with movups is referred to by its first read";
    EXPECT_EQ(found[0].Address, Base + HandlerC) << "and the plain name read with mov at two offsets still marks it a registration";

    EXPECT_EQ(found[1].Owner, "Loose");
    EXPECT_EQ(found[1].Handler, "MSG_NoHandler");
    EXPECT_EQ(found[1].Site, Base + Lonely + 0x07);
    EXPECT_EQ(found[1].Address, 0u) << "no function address is loaded before it, so none is guessed";

    EXPECT_EQ(found[2].Owner, "Window");
    EXPECT_EQ(found[2].Handler, "MSG_Ping");
    EXPECT_EQ(found[2].Site, Base + Registrar + 0x0E) << "a second reference to the same name belongs to the registration before it";
    EXPECT_EQ(found[2].Address, Base + HandlerA) << "the plain name read between them is data, not a function, and the handler naming itself registers nothing";

    EXPECT_EQ(found[3].Owner, "ns::Other<int>");
    EXPECT_EQ(found[3].Handler, "MSG_Pong");
    EXPECT_EQ(found[3].Site, Base + Registrar + 0x2A) << "a plain name the linker kept only as the tail of the debug name still counts";
    EXPECT_EQ(found[3].Address, Base + HandlerB);

    EXPECT_EQ(found[4].Owner, "Poster");
    EXPECT_EQ(found[4].Handler, "MSG_PostX");
    EXPECT_EQ(found[4].Address, Base + HandlerX) << "a handler passed to the registering call after its name is found after it";
    EXPECT_EQ(found[5].Handler, "MSG_PostY");
    EXPECT_EQ(found[5].Address, Base + HandlerY) << "and the previous registration's handler, loaded before this name, is not taken for it";
}

TEST(MessageHandlersTest, ADebugNameIsAnOwnerAndAMessageNameAroundTheLastSeparator)
{
    EXPECT_EQ(MessageHandlers::SplitName("Window::MSG_Ping"), 6u);
    EXPECT_EQ(MessageHandlers::SplitName("ns::Other<int>::MSG_Pong"), 14u);
    EXPECT_EQ(MessageHandlers::SplitName("Window::MSG_Ping::MSG_Pong"), 16u);
    EXPECT_FALSE(MessageHandlers::SplitName("MSG_Ping"));
    EXPECT_FALSE(MessageHandlers::SplitName("::MSG_Ping"));
    EXPECT_FALSE(MessageHandlers::SplitName("Window::MSG_"));
    EXPECT_FALSE(MessageHandlers::SplitName("Window::MSG_Ping now"));
    EXPECT_FALSE(MessageHandlers::SplitName("1Window::MSG_Ping"));
    EXPECT_FALSE(MessageHandlers::SplitName("Window MSG::MSG_Ping"));
}
