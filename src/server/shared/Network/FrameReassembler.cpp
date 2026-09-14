/*
 * Project Ambrose by Imjustchico
 * Buffers incoming bytes, sizes each frame from its prefix, validates chained DML lengths, and compacts consumed bytes.
 */

#include "FrameReassembler.h"

FrameReassembler::FrameReassembler(FrameLimits limits) : _limits(limits)
{
}

void FrameReassembler::Feed(std::span<uint8 const> bytes)
{
    if (HasError() || bytes.empty())
        return;
    _buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
}

std::optional<Frame> FrameReassembler::Next()
{
    if (HasError())
        return std::nullopt;

    std::span<uint8 const> const available(_buffer.data() + _offset, _buffer.size() - _offset);
    if (!_pendingSize)
    {
        FrameError error = FrameError::None;
        _pendingSize = FrameLayout::GetFrameSize(available, _limits.LongLength, error);
        if (error != FrameError::None)
        {
            _error = error;
            return std::nullopt;
        }
        if (!_pendingSize)
        {
            Compact();
            return std::nullopt;
        }
    }
    std::optional<uint64> const size = _pendingSize;
    if (*size > _limits.MaxFrameSize)
    {
        _error = FrameError::TooLarge;
        return std::nullopt;
    }
    std::size_t const frameSize = static_cast<std::size_t>(*size);
    if (available.size() < frameSize)
    {
        Compact();
        return std::nullopt;
    }

    bool const isLong = available[2] == (FrameLayout::LongLengthMarker & 0xFF) && available[3] == (FrameLayout::LongLengthMarker >> 8);
    std::size_t const header = isLong ? FrameLayout::LongPrefixSize : FrameLayout::PrefixSize;
    std::span<uint8 const> const payload = available.subspan(header + FrameLayout::FrameHeaderSize, frameSize - header - FrameLayout::FrameHeaderSize - FrameLayout::TrailerSize);

    Frame frame;
    frame.IsControl = available[header] == 1;
    frame.Opcode = available[header + 1];
    frame.Reserved = static_cast<uint16>(available[header + 2] | (available[header + 3] << 8));
    frame.Trailer = available[frameSize - 1];
    frame.IsLong = isLong;
    if (!frame.IsControl)
    {
        FrameError const dmlError = FrameLayout::ValidateDmlPayload(payload, _limits.MaxDmlMessages);
        if (dmlError != FrameError::None)
        {
            _error = dmlError;
            return std::nullopt;
        }
    }
    frame.Payload.assign(payload.begin(), payload.end());
    _offset += frameSize;
    _pendingSize.reset();
    Compact();
    return frame;
}

void FrameReassembler::Reset()
{
    _buffer.clear();
    _buffer.shrink_to_fit();
    _offset = 0;
    _pendingSize.reset();
    _error = FrameError::None;
}

void FrameReassembler::Compact()
{
    if (_offset == 0)
        return;
    if (_offset == _buffer.size())
    {
        _offset = 0;
        if (_buffer.capacity() > RetainedCapacity)
            std::vector<uint8>().swap(_buffer);
        else
            _buffer.clear();
        return;
    }
    if (_offset >= _buffer.size() / 2)
    {
        _buffer.erase(_buffer.begin(), _buffer.begin() + static_cast<std::ptrdiff_t>(_offset));
        _offset = 0;
        if (_buffer.capacity() > RetainedCapacity && _buffer.capacity() > 4 * _buffer.size())
            _buffer.shrink_to_fit();
    }
}
