/*
 * Project Ambrose by Imjustchico
 * Builds byte-exact KI control and DML frames, switching to the long-length form behind one rule for bodies over 0x777F bytes, including a DML frame whose body is encoded straight into the output.
 */

#ifndef AMBROSE_FRAMEWRITER_H
#define AMBROSE_FRAMEWRITER_H

#include "ByteBuffer.h"
#include "Frame.h"

#include <span>

class FrameWriter
{
public:
    static void WriteControl(ByteBuffer& out, uint8 opcode, std::span<uint8 const> body, LongFrameLength longLength = LongFrameLength::BodyOnly);
    static void WriteDml(ByteBuffer& out, uint8 serviceId, uint8 order, std::span<uint8 const> body, LongFrameLength longLength = LongFrameLength::BodyOnly);
    static void WriteDml(ByteBuffer& out, std::span<DmlMessageData const> messages, LongFrameLength longLength = LongFrameLength::BodyOnly);
    static void WriteFrame(ByteBuffer& out, Frame const& frame, LongFrameLength longLength = LongFrameLength::BodyOnly);
    static std::size_t BeginDml(ByteBuffer& out, uint8 serviceId, uint8 order);
    static void EndDml(ByteBuffer& out, std::size_t start, LongFrameLength longLength = LongFrameLength::BodyOnly);

private:
    static void WritePrefix(ByteBuffer& out, Frame const& frame, bool isLong, LongFrameLength longLength);
};

#endif
