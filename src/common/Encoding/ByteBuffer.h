/*
 * Project Ambrose by Imjustchico
 * Growable little-endian byte buffer with bounds-checked typed reads, writes, and in-place patches.
 */

#ifndef AMBROSE_BYTEBUFFER_H
#define AMBROSE_BYTEBUFFER_H

#include "Types.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

class ByteBufferException : public std::out_of_range
{
public:
    ByteBufferException(std::size_t position, std::size_t requested, std::size_t size);

    std::size_t GetPosition() const { return _position; }
    std::size_t GetRequested() const { return _requested; }
    std::size_t GetSize() const { return _size; }

private:
    std::size_t _position;
    std::size_t _requested;
    std::size_t _size;
};

template<typename T>
concept ByteBufferScalar = (std::is_integral_v<T> || std::is_floating_point_v<T>) && !std::is_same_v<T, bool>;

class ByteBuffer
{
public:
    ByteBuffer() = default;
    explicit ByteBuffer(std::vector<uint8> data);
    explicit ByteBuffer(std::span<uint8 const> data);

    template<ByteBufferScalar T>
    void Write(T value)
    {
        uint8 bytes[sizeof(T)];
        EncodeLittleEndian(value, bytes);
        _storage.insert(_storage.end(), bytes, bytes + sizeof(T));
    }

    template<ByteBufferScalar T>
    T Read()
    {
        EnsureReadable(sizeof(T));
        T const value = DecodeLittleEndian<T>(_storage.data() + _readPosition);
        _readPosition += sizeof(T);
        return value;
    }

    template<ByteBufferScalar T>
    void Put(std::size_t position, T value)
    {
        if (position > _storage.size() || sizeof(T) > _storage.size() - position)
            throw ByteBufferException(position, sizeof(T), _storage.size());
        EncodeLittleEndian(value, _storage.data() + position);
    }

    void WriteBytes(std::span<uint8 const> bytes);
    std::span<uint8 const> ReadBytes(std::size_t count);
    void Skip(std::size_t count);
    void EnsureReadable(std::size_t count) const;

    std::size_t GetSize() const { return _storage.size(); }
    std::size_t GetReadPosition() const { return _readPosition; }
    std::size_t GetRemaining() const { return _storage.size() - _readPosition; }
    std::span<uint8 const> GetData() const { return _storage; }

    void SetReadPosition(std::size_t position);
    void Reserve(std::size_t capacity);
    void Clear();

private:
    template<ByteBufferScalar T>
    static void EncodeLittleEndian(T value, uint8* out)
    {
        std::memcpy(out, &value, sizeof(T));
        if constexpr (std::endian::native == std::endian::big)
            std::reverse(out, out + sizeof(T));
    }

    template<ByteBufferScalar T>
    static T DecodeLittleEndian(uint8 const* in)
    {
        uint8 bytes[sizeof(T)];
        std::memcpy(bytes, in, sizeof(T));
        if constexpr (std::endian::native == std::endian::big)
            std::reverse(bytes, bytes + sizeof(T));
        T value;
        std::memcpy(&value, bytes, sizeof(T));
        return value;
    }

    std::vector<uint8> _storage;
    std::size_t _readPosition = 0;
};

#endif
