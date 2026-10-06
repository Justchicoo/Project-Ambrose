/*
 * Project Ambrose by Imjustchico
 * Exercises fragmented frame reassembly with arbitrary network bytes.
 */

#include "AllocationCeiling.h"
#include "FrameReassembler.h"
#include "FrameWriter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
    void WriteSeeds(std::filesystem::path const& folder)
    {
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error)
            std::abort();
        ByteBuffer dml;
        ByteBuffer control;
        FrameWriter::WriteDml(dml, 9, 1, std::span<uint8 const>());
        FrameWriter::WriteControl(control, 4, std::span<uint8 const>());
        std::array<ByteBuffer const*, 2> const seeds{ &dml, &control };
        for (std::size_t index = 0; index < seeds.size(); ++index)
        {
            std::ofstream stream(folder / ("seed-" + std::to_string(index) + ".bin"), std::ios::binary | std::ios::trunc);
            if (!stream)
                std::abort();
            stream.write(reinterpret_cast<char const*>(seeds[index]->GetData().data()), static_cast<std::streamsize>(seeds[index]->GetSize()));
            if (!stream)
                std::abort();
        }
    }
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    AllocationCeiling::Install();
    for (int index = 1; index < *argc; ++index)
    {
        std::string_view const argument = (*argv)[index];
        if (!argument.empty() && argument.front() != '-')
        {
            WriteSeeds(std::filesystem::path(argument));
            break;
        }
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, std::size_t size)
{
    AllocationCeiling::Scope const ceiling;
    FrameLimits limits;
    FrameReassembler reassembler(limits);
    constexpr std::size_t FragmentSize = 1024;

    for (std::size_t offset = 0; offset < size && !reassembler.HasError(); offset += FragmentSize)
    {
        std::size_t const count = std::min(FragmentSize, size - offset);
        reassembler.Feed(std::span<uint8 const>(data + offset, count));
        while (std::optional<Frame> frame = reassembler.Next())
        {
            if (frame->Payload.size() > limits.MaxFrameSize)
                std::abort();
        }
        if (reassembler.GetBufferCapacity() > limits.MaxFrameSize)
            std::abort();
    }
    return 0;
}
