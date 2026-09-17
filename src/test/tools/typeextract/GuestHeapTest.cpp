/*
 * Project Ambrose by Imjustchico
 * Tests the guest heap: block alignment and gaps, remembered sizes, zero-filled blocks, reallocation copies, exhaustion without overflow, the contained range, free semantics and refused regions.
 */

#include "GuestHeap.h"
#include "Machine.h"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 HeapBase = 0x10000000000;

    std::string HeapError(GuestHeap& heap, uint64 size)
    {
        try
        {
            heap.Allocate(size, false);
        }
        catch (EmulationError const& error)
        {
            return error.what();
        }
        return {};
    }

    void CreateHeap(Machine& machine, uint64 base, uint64 size)
    {
        GuestHeap const heap(machine, base, size);
    }
}

TEST(GuestHeapTest, AlignsBlocksAndLeavesAGapAfterEach)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase, 0x10000);
    EXPECT_TRUE(machine.IsMapped(HeapBase, 0x10000));
    EXPECT_EQ(heap.GetBase(), HeapBase);
    EXPECT_EQ(heap.GetTop(), HeapBase);
    EXPECT_EQ(heap.GetLimit(), HeapBase + 0x10000);
    EXPECT_EQ(heap.GetAllocationCount(), uint64{ 0 });

    EXPECT_EQ(heap.Allocate(1, false), HeapBase);
    EXPECT_EQ(heap.GetTop(), HeapBase + 32);
    EXPECT_EQ(heap.Allocate(16, false), HeapBase + 32);
    EXPECT_EQ(heap.GetTop(), HeapBase + 64);
    EXPECT_EQ(heap.Allocate(17, false), HeapBase + 64);
    EXPECT_EQ(heap.GetTop(), HeapBase + 112);
    EXPECT_EQ(heap.Allocate(0, false), HeapBase + 112);
    EXPECT_EQ(heap.GetTop(), HeapBase + 144);
    EXPECT_EQ(heap.GetAllocationCount(), uint64{ 4 });

    uint64 previous = heap.GetTop();
    for (uint64 size = 1; size < 200; size += 7)
    {
        uint64 const address = heap.Allocate(size, false);
        EXPECT_EQ(address, previous);
        EXPECT_EQ(address % GuestHeap::Alignment, uint64{ 0 });
        EXPECT_GE(heap.GetTop() - address, size + GuestHeap::BlockGap);
        EXPECT_LT(heap.GetTop() - address, size + GuestHeap::BlockGap + GuestHeap::Alignment);
        previous = heap.GetTop();
    }
}

TEST(GuestHeapTest, UnalignedBaseStartsAtTheNextAlignedAddress)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase + 3, 0x1000);
    EXPECT_EQ(heap.GetBase(), HeapBase + 3);
    EXPECT_EQ(heap.GetLimit(), HeapBase + 3 + 0x1000);
    EXPECT_EQ(heap.Allocate(5, false), HeapBase + 16);
    EXPECT_EQ(heap.GetTop(), HeapBase + 48);
}

TEST(GuestHeapTest, RemembersEachBlocksSize)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase, 0x10000);
    uint64 const small = heap.Allocate(3, false);
    uint64 const empty = heap.Allocate(0, false);
    uint64 const large = heap.Allocate(0x1234, true);

    EXPECT_EQ(heap.SizeOf(small), uint64{ 3 });
    EXPECT_EQ(heap.SizeOf(empty), uint64{ 1 });
    EXPECT_EQ(heap.SizeOf(large), uint64{ 0x1234 });
    EXPECT_FALSE(heap.SizeOf(small + 1).has_value());
    EXPECT_FALSE(heap.SizeOf(0).has_value());
}

TEST(GuestHeapTest, BlocksReadAsZero)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase, 0x100000);

    uint64 const fresh = heap.Allocate(64, false);
    EXPECT_EQ(machine.ReadBytes(fresh, 64), std::vector<uint8>(64, 0));

    std::vector<uint8> const dirt(0x30000, 0xAB);
    machine.Write(heap.GetTop(), dirt);
    uint64 const zeroed = heap.Allocate(0x20000, true);
    EXPECT_EQ(machine.ReadBytes(zeroed, 0x20000), std::vector<uint8>(0x20000, 0));
}

TEST(GuestHeapTest, ReallocationCopiesTheKeptBytes)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase, 0x10000);

    uint64 const original = heap.Allocate(8, false);
    std::vector<uint8> const content = { 1, 2, 3, 4, 5, 6, 7, 8 };
    machine.Write(original, content);

    uint64 const grown = heap.Reallocate(original, 32, false);
    EXPECT_NE(grown, original);
    EXPECT_GT(grown, original);
    EXPECT_EQ(machine.ReadBytes(grown, 8), content);
    EXPECT_EQ(machine.ReadBytes(grown + 8, 24), std::vector<uint8>(24, 0));
    EXPECT_EQ(heap.SizeOf(grown), uint64{ 32 });
    EXPECT_FALSE(heap.SizeOf(original).has_value());
    EXPECT_FALSE(heap.Free(original));

    uint64 const shrunk = heap.Reallocate(grown, 4, false);
    EXPECT_EQ(machine.ReadBytes(shrunk, 4), (std::vector<uint8>{ 1, 2, 3, 4 }));
    EXPECT_EQ(heap.SizeOf(shrunk), uint64{ 4 });

    std::vector<uint8> const dirt(128, 0xCD);
    machine.Write(heap.GetTop(), dirt);
    uint64 const zeroed = heap.Reallocate(shrunk, 64, true);
    EXPECT_EQ(machine.ReadBytes(zeroed, 4), (std::vector<uint8>{ 1, 2, 3, 4 }));
    EXPECT_EQ(machine.ReadBytes(zeroed + 4, 60), std::vector<uint8>(60, 0));

    uint64 const nothing = heap.Reallocate(zeroed, 0, false);
    EXPECT_EQ(heap.SizeOf(nothing), uint64{ 1 });
    EXPECT_EQ(machine.ReadU8(nothing), 1);

    uint64 const top = heap.GetTop();
    uint64 const created = heap.Reallocate(0, 10, false);
    EXPECT_EQ(created, top);
    EXPECT_EQ(heap.SizeOf(created), uint64{ 10 });
    EXPECT_EQ(heap.GetAllocationCount(), uint64{ 6 });

    try
    {
        heap.Reallocate(created + 1, 4, false);
        ADD_FAILURE() << "reallocating an unknown address succeeded";
    }
    catch (EmulationError const& error)
    {
        std::string const message = error.what();
        EXPECT_NE(message.find("0x10000000"), std::string::npos) << message;
        EXPECT_NE(message.find("not a live block"), std::string::npos) << message;
    }
    EXPECT_EQ(heap.GetAllocationCount(), uint64{ 6 });
}

TEST(GuestHeapTest, ThrowsWhenExhaustedWithoutMovingTheTop)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase, 0x1000);

    EXPECT_EQ(heap.Allocate(0x1000 - GuestHeap::BlockGap, false), HeapBase);
    EXPECT_EQ(heap.GetTop(), heap.GetLimit());
    std::string const message = HeapError(heap, 1);
    EXPECT_NE(message.find("the guest heap of 4096 bytes is exhausted"), std::string::npos) << message;
    EXPECT_EQ(heap.GetTop(), heap.GetLimit());
    EXPECT_EQ(heap.GetAllocationCount(), uint64{ 1 });

    GuestHeap second(machine, HeapBase + 0x10000, 0x1000);
    EXPECT_NE(HeapError(second, std::numeric_limits<uint64>::max()).find("exhausted"), std::string::npos);
    EXPECT_NE(HeapError(second, std::numeric_limits<uint64>::max() - 8).find("exhausted"), std::string::npos);
    EXPECT_NE(HeapError(second, 0x1000 - GuestHeap::BlockGap + 1).find("exhausted"), std::string::npos);
    EXPECT_EQ(second.GetTop(), HeapBase + 0x10000);
    EXPECT_EQ(second.GetAllocationCount(), uint64{ 0 });

    GuestHeap padded(machine, HeapBase + 0x20000, 0x1008);
    EXPECT_NE(HeapError(padded, 0x1008 - GuestHeap::BlockGap - 1).find("exhausted"), std::string::npos);
    EXPECT_EQ(padded.Allocate(0x1008 - GuestHeap::BlockGap - 8, false), HeapBase + 0x20000);
    EXPECT_EQ(padded.GetTop(), HeapBase + 0x21000);
    EXPECT_NE(HeapError(padded, 1).find("exhausted"), std::string::npos);
}

TEST(GuestHeapTest, ContainsCoversTheUsedRange)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase, 0x1000);
    EXPECT_FALSE(heap.Contains(HeapBase));

    uint64 const block = heap.Allocate(10, false);
    EXPECT_TRUE(heap.Contains(block));
    EXPECT_TRUE(heap.Contains(block + 9));
    EXPECT_TRUE(heap.Contains(block + 10));
    EXPECT_TRUE(heap.Contains(heap.GetTop() - 1));
    EXPECT_FALSE(heap.Contains(heap.GetTop()));
    EXPECT_FALSE(heap.Contains(HeapBase - 1));
    EXPECT_FALSE(heap.Contains(heap.GetLimit()));
    EXPECT_FALSE(heap.Contains(0));
}

TEST(GuestHeapTest, FreeForgetsBlocksWithoutReusingTheirMemory)
{
    Machine machine;
    GuestHeap heap(machine, HeapBase, 0x1000);

    uint64 const first = heap.Allocate(16, false);
    uint64 const second = heap.Allocate(16, false);
    EXPECT_TRUE(heap.Free(first));
    EXPECT_FALSE(heap.Free(first));
    EXPECT_FALSE(heap.SizeOf(first).has_value());
    EXPECT_EQ(heap.SizeOf(second), uint64{ 16 });
    EXPECT_FALSE(heap.Free(0));
    EXPECT_FALSE(heap.Free(second + 1));
    EXPECT_TRUE(heap.Contains(first));

    uint64 const third = heap.Allocate(16, false);
    EXPECT_GT(third, second);
    EXPECT_EQ(heap.GetAllocationCount(), uint64{ 3 });
    EXPECT_THROW(heap.Reallocate(first, 32, false), EmulationError);
}

TEST(GuestHeapTest, RefusesRegionsThatCannotHoldAHeap)
{
    Machine machine;
    EXPECT_THROW(CreateHeap(machine, HeapBase, 0), EmulationError);
    EXPECT_THROW(CreateHeap(machine, std::numeric_limits<uint64>::max() - 0xFFF, 0x1000), EmulationError);
    EXPECT_THROW(CreateHeap(machine, HeapBase + 1, 15), EmulationError);

    GuestHeap heap(machine, HeapBase, 0x2000);
    EXPECT_THROW(CreateHeap(machine, HeapBase + 0x1000, 0x2000), EmulationError);
    EXPECT_NO_THROW(CreateHeap(machine, HeapBase + 0x2000, 0x1000));
}
