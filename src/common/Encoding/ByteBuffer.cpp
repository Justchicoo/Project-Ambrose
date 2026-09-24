/*
 * Project Ambrose by Imjustchico
 * Implements byte buffer bounds checks, raw byte access and insertion, releasing the storage, and the overrun exception message.
 */

#include "ByteBuffer.h"

#include <fmt/format.h>

#include <utility>

ByteBufferException::ByteBufferException(std::size_t position, std::size_t requested, std::size_t size)
    : std::out_of_range(fmt::format("byte buffer overrun: position {} needs {} bytes but size is {}", position, requested, size)),
      _position(position),
      _requested(requested),
      _size(size)
{
}

ByteBuffer::ByteBuffer(std::vector<uint8> data) : _storage(std::move(data))
{
}

ByteBuffer::ByteBuffer(std::span<uint8 const> data) : _storage(data.begin(), data.end())
{
}

void ByteBuffer::WriteBytes(std::span<uint8 const> bytes)
{
    _storage.insert(_storage.end(), bytes.begin(), bytes.end());
}

void ByteBuffer::InsertBytes(std::size_t position, std::span<uint8 const> bytes)
{
    if (position > _storage.size())
        throw ByteBufferException(position, 0, _storage.size());
    _storage.insert(_storage.begin() + static_cast<std::ptrdiff_t>(position), bytes.begin(), bytes.end());
}

std::span<uint8 const> ByteBuffer::ReadBytes(std::size_t count)
{
    EnsureReadable(count);
    std::span<uint8 const> const view(_storage.data() + _readPosition, count);
    _readPosition += count;
    return view;
}

void ByteBuffer::Skip(std::size_t count)
{
    EnsureReadable(count);
    _readPosition += count;
}

void ByteBuffer::EnsureReadable(std::size_t count) const
{
    if (count > _storage.size() - _readPosition)
        throw ByteBufferException(_readPosition, count, _storage.size());
}

void ByteBuffer::SetReadPosition(std::size_t position)
{
    if (position > _storage.size())
        throw ByteBufferException(position, 0, _storage.size());
    _readPosition = position;
}

void ByteBuffer::Reserve(std::size_t capacity)
{
    _storage.reserve(capacity);
}

void ByteBuffer::Clear()
{
    _storage.clear();
    _readPosition = 0;
}

std::vector<uint8> ByteBuffer::Release() noexcept
{
    std::vector<uint8> released = std::move(_storage);
    _storage = {};
    _readPosition = 0;
    return released;
}
