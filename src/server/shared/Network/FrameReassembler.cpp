/*
 * Project Ambrose by Imjustchico
 * Buffers incoming bytes, sizes each frame from its prefix, validates chained DML lengths, and compacts consumed bytes.
 */

#include "FrameReassembler.h"

#include <algorithm>

FrameReassembler::FrameReassembler(FrameLimits limits) : _limits(limits)
{
}

void FrameReassembler::SetLimits(FrameLimits limits)
{
    _limits = limits;
    if (GetBufferedSize() > _limits.MaxFrameSize)
    {
        _error = FrameError::TooLarge;
        return;
    }
    if (_buffer.capacity() <= _limits.MaxFrameSize)
        return;
    if (_offset != 0)
    {
        _buffer.erase(_buffer.begin(), _buffer.begin() + static_cast<std::ptrdiff_t>(_offset));
        _offset = 0;
    }
    std::vector<uint8> bounded;
    bounded.reserve(_buffer.size());
    bounded.insert(bounded.end(), _buffer.begin(), _buffer.end());
    _buffer.swap(bounded);
}

void FrameReassembler::Feed(std::span<uint8 const> bytes)
{
    if (HasError() || bytes.empty())
        return;

    std::size_t offset = 0;
    while (offset < bytes.size() && !HasError())
    {
        if (_offset != 0)
        {
            _buffer.erase(_buffer.begin(), _buffer.begin() + static_cast<std::ptrdiff_t>(_offset));
            _offset = 0;
        }
        std::size_t const buffered = GetBufferedSize();
        if (buffered >= _limits.MaxFrameSize)
        {
            _error = FrameError::TooLarge;
            return;
        }
        std::size_t const count = std::min(bytes.size() - offset, _limits.MaxFrameSize - buffered);
        std::size_t const required = _buffer.size() + count;
        if (required > _buffer.capacity())
            _buffer.reserve(required);
        _buffer.insert(_buffer.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + count));
        offset += count;

        while (std::optional<Frame> frame = ParseNext())
            _readyFrames.push_back(std::move(*frame));
    }
}

std::optional<Frame> FrameReassembler::Next()
{
    if (!_readyFrames.empty())
    {
        Frame frame = std::move(_readyFrames.front());
        _readyFrames.pop_front();
        std::size_t const prefix = frame.IsLong ? FrameLayout::LongPrefixSize : FrameLayout::PrefixSize;
        std::size_t const frameSize = prefix + FrameLayout::FrameHeaderSize + frame.Payload.size() + FrameLayout::TrailerSize;
        if (frameSize > _limits.MaxFrameSize)
        {
            _error = FrameError::TooLarge;
            return std::nullopt;
        }
        if (!frame.IsControl)
        {
            FrameError const error = FrameLayout::ValidateDmlPayload(frame.Payload, _limits.MaxDmlMessages);
            if (error != FrameError::None)
            {
                _error = error;
                return std::nullopt;
            }
        }
        return frame;
    }
    return ParseNext();
}

std::optional<Frame> FrameReassembler::ParseNext()
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
    _readyFrames.clear();
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
