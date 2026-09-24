/*
 * Project Ambrose by Imjustchico
 * Streams zlib inflate and deflate in bounded chunks, reserving output in proportion to the input rather than any declared size and stopping as soon as output would exceed the caller's cap.
 */

#include "Compression.h"

#include <zlib.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>

namespace
{
    constexpr std::size_t ChunkSize = 64 * 1024;
    constexpr uInt MaxZlibCount = std::numeric_limits<uInt>::max();

    int WindowBits(Ambrose::Compression::Format format)
    {
        switch (format)
        {
            case Ambrose::Compression::Format::Zlib: return 15;
            case Ambrose::Compression::Format::Gzip: return 15 + 16;
            case Ambrose::Compression::Format::Raw: return -15;
            case Ambrose::Compression::Format::Auto: return 15 + 32;
        }
        return 15;
    }

    class InflateStream
    {
    public:
        explicit InflateStream(int windowBits)
        {
            _stream.zalloc = Z_NULL;
            _stream.zfree = Z_NULL;
            _stream.opaque = Z_NULL;
            _initialized = inflateInit2(&_stream, windowBits) == Z_OK;
        }

        ~InflateStream()
        {
            if (_initialized)
                inflateEnd(&_stream);
        }

        InflateStream(InflateStream const&) = delete;
        InflateStream& operator=(InflateStream const&) = delete;

        bool IsInitialized() const noexcept { return _initialized; }
        z_stream& Get() noexcept { return _stream; }

    private:
        z_stream _stream{};
        bool _initialized = false;
    };
}

static Ambrose::Compression::InflateResult InflateWithReserve(std::span<uint8 const> input, std::size_t maxOutputSize, Ambrose::Compression::Format format, bool allowTrailingData, std::size_t reserve);

namespace
{
    std::size_t InitialReserve(std::size_t inputSize, std::size_t maxOutputSize) noexcept
    {
        std::size_t const scaled = inputSize > std::numeric_limits<std::size_t>::max() / 4 ? maxOutputSize : inputSize * 4;
        return std::min(maxOutputSize, std::max(scaled, ChunkSize));
    }
}

Ambrose::Compression::InflateResult Ambrose::Compression::Inflate(std::span<uint8 const> input, std::size_t maxOutputSize, Format format, bool allowTrailingData)
{
    return InflateWithReserve(input, maxOutputSize, format, allowTrailingData, InitialReserve(input.size(), maxOutputSize));
}

static Ambrose::Compression::InflateResult InflateWithReserve(std::span<uint8 const> input, std::size_t maxOutputSize, Ambrose::Compression::Format format, bool allowTrailingData, std::size_t reserve)
{
    using namespace Ambrose::Compression;
    InflateResult result;
    auto const fail = [&result](Status status) -> InflateResult&
    {
        result.Code = status;
        result.Data.clear();
        result.Data.shrink_to_fit();
        return result;
    };
    InflateStream stream(WindowBits(format));
    if (!stream.IsInitialized())
        return fail(Status::OutOfMemory);
    z_stream& z = stream.Get();
    std::size_t inputOffset = 0;
    std::size_t outputSize = 0;
    try
    {
        result.Data.reserve(reserve);
        while (true)
        {
            if (z.avail_in == 0 && inputOffset < input.size())
            {
                std::size_t const take = std::min<std::size_t>(input.size() - inputOffset, MaxZlibCount);
                z.next_in = const_cast<Bytef*>(input.data() + inputOffset);
                z.avail_in = static_cast<uInt>(take);
                inputOffset += take;
            }
            std::size_t const room = maxOutputSize - outputSize;
            std::size_t const chunk = room >= ChunkSize ? ChunkSize : room + 1;
            result.Data.resize(outputSize + chunk);
            z.next_out = result.Data.data() + outputSize;
            z.avail_out = static_cast<uInt>(chunk);
            int const code = inflate(&z, Z_NO_FLUSH);
            std::size_t const produced = chunk - z.avail_out;
            outputSize += produced;
            result.Data.resize(outputSize);
            if (outputSize > maxOutputSize)
                return fail(Status::OutputTooLarge);
            if (code == Z_STREAM_END)
                break;
            if (code == Z_NEED_DICT || code == Z_DATA_ERROR || code == Z_STREAM_ERROR)
                return fail(Status::Corrupt);
            if (code == Z_MEM_ERROR)
                return fail(Status::OutOfMemory);
            if (produced == 0 && z.avail_in == 0 && inputOffset >= input.size())
                return fail(Status::Truncated);
        }
    }
    catch (std::exception const&)
    {
        return fail(Status::OutOfMemory);
    }
    result.ConsumedInput = inputOffset - z.avail_in;
    if (!allowTrailingData && result.ConsumedInput != input.size())
        return fail(Status::TrailingData);
    if (result.Data.capacity() > result.Data.size() + result.Data.size() / 2 + ChunkSize)
        result.Data.shrink_to_fit();
    return result;
}

Ambrose::Compression::InflateResult Ambrose::Compression::InflateExact(std::span<uint8 const> input, std::size_t expectedSize, Format format)
{
    InflateResult result = InflateWithReserve(input, expectedSize, format, false, InitialReserve(input.size(), expectedSize));
    if (result.Succeeded() && result.Data.size() != expectedSize)
    {
        result.Code = Status::SizeMismatch;
        result.Data.clear();
    }
    return result;
}

std::vector<uint8> Ambrose::Compression::Deflate(std::span<uint8 const> input, int level, Format format)
{
    if (format == Format::Auto)
        format = Format::Zlib;
    z_stream z{};
    if (deflateInit2(&z, std::clamp(level, 0, 9), Z_DEFLATED, WindowBits(format), 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("deflateInit2 failed");
    struct DeflateEnd
    {
        z_stream& Stream;
        ~DeflateEnd() { deflateEnd(&Stream); }
    } const cleanup{ z };
    std::vector<uint8> output;
    std::size_t inputOffset = 0;
    int code = Z_OK;
    do
    {
        if (z.avail_in == 0 && inputOffset < input.size())
        {
            std::size_t const take = std::min<std::size_t>(input.size() - inputOffset, MaxZlibCount);
            z.next_in = const_cast<Bytef*>(input.data() + inputOffset);
            z.avail_in = static_cast<uInt>(take);
            inputOffset += take;
        }
        std::size_t const before = output.size();
        output.resize(before + ChunkSize);
        z.next_out = output.data() + before;
        z.avail_out = static_cast<uInt>(ChunkSize);
        code = deflate(&z, inputOffset >= input.size() ? Z_FINISH : Z_NO_FLUSH);
        output.resize(before + ChunkSize - z.avail_out);
    } while (code == Z_OK || code == Z_BUF_ERROR);
    if (code != Z_STREAM_END)
        throw std::runtime_error("deflate failed");
    return output;
}

std::string_view Ambrose::Compression::GetStatusName(Status status) noexcept
{
    switch (status)
    {
        case Status::Ok: return "ok";
        case Status::OutputTooLarge: return "output exceeds the size limit";
        case Status::SizeMismatch: return "output size does not match the expected size";
        case Status::Truncated: return "input ends before the stream does";
        case Status::Corrupt: return "stream is corrupt";
        case Status::TrailingData: return "data follows the end of the stream";
        case Status::OutOfMemory: return "out of memory";
    }
    return "unknown";
}
