/*
 * Project Ambrose by Imjustchico
 * Feeds seeded random mutations of the fuzz corpus's seeds, compact and versionable, with fixed or compact lengths, bare and enveloped, to the decoder, a million in AddressSanitizer builds and a hundred thousand otherwise unless AMBROSE_FUZZ_ITERATIONS says, and checks every one either decodes to an object that re-encodes and decodes back equal or is refused, never reading past its input, making an allocation larger than the limits allow or allocating more in total than the memory budget and inflation limit allow, plus a list that would outgrow the budget refused before it allocates and a huge list count in a 10-byte blob refused before anything is allocated.
 */

#include "AllocationCounter.h"
#include "Environment.h"
#include "ObjectFuzzCorpus.h"
#include "StringHash.h"
#include "StringUtil.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
#ifdef AMBROSE_SANITIZE_ADDRESS
    constexpr uint64 DefaultIterations = 1000000;
#else
    constexpr uint64 DefaultIterations = 100000;
#endif

    class Mutator
    {
    public:
        Mutator(uint64 seed, std::vector<ObjectFuzzCorpus::Seed> const& corpus, std::vector<uint32> hashes) : _random(seed), _corpus(corpus), _hashes(std::move(hashes))
        {
        }

        ObjectFuzzCorpus::Seed Next()
        {
            ObjectFuzzCorpus::Seed input = _corpus[Pick(_corpus.size())];
            if (Pick(16) == 0)
                input.Mode = static_cast<uint8>(input.Mode ^ (1u << Pick(ObjectFuzzCorpus::ModeBits)));
            std::size_t const rounds = 1 + Pick(4);
            for (std::size_t round = 0; round < rounds; ++round)
                Mutate(input.Bytes);
            return input;
        }

    private:
        std::size_t Pick(std::size_t count)
        {
            return count == 0 ? 0 : static_cast<std::size_t>(_random() % count);
        }

        static std::ptrdiff_t At(std::size_t index) noexcept
        {
            return static_cast<std::ptrdiff_t>(index);
        }

        static void PutU32(std::vector<uint8>& blob, std::size_t at, uint32 value)
        {
            for (std::size_t index = 0; index < 4 && at + index < blob.size(); ++index)
                blob[at + index] = static_cast<uint8>(value >> (8 * index));
        }

        void Mutate(std::vector<uint8>& blob)
        {
            static constexpr std::array<uint32, 8> Interesting{ 0u, 1u, 0x7Fu, 0xFFFFu, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0x10000u };
            switch (Pick(9))
            {
                case 0:
                    if (!blob.empty())
                        blob[Pick(blob.size())] ^= static_cast<uint8>(1u << Pick(8));
                    break;
                case 1:
                    if (!blob.empty())
                        blob[Pick(blob.size())] = static_cast<uint8>(_random());
                    break;
                case 2:
                    blob.resize(Pick(blob.size() + 1));
                    break;
                case 3:
                {
                    std::vector<uint8> extra(1 + Pick(8));
                    for (uint8& byte : extra)
                        byte = static_cast<uint8>(_random());
                    blob.insert(blob.begin() + At(Pick(blob.size() + 1)), extra.begin(), extra.end());
                    break;
                }
                case 4:
                    if (!blob.empty())
                    {
                        std::size_t const at = Pick(blob.size());
                        std::size_t const count = std::min(blob.size() - at, 1 + Pick(16));
                        blob.erase(blob.begin() + At(at), blob.begin() + At(at + count));
                    }
                    break;
                case 5:
                    if (blob.size() >= 4)
                        PutU32(blob, Pick(blob.size() - 3), Interesting[Pick(Interesting.size())]);
                    break;
                case 6:
                    if (blob.size() >= 4 && !_hashes.empty())
                        PutU32(blob, Pick(blob.size() - 3), _hashes[Pick(_hashes.size())]);
                    break;
                case 7:
                    if (!blob.empty())
                    {
                        std::size_t const at = Pick(blob.size());
                        std::size_t const count = std::min(blob.size() - at, 1 + Pick(32));
                        std::vector<uint8> const chunk(blob.begin() + At(at), blob.begin() + At(at + count));
                        blob.insert(blob.begin() + At(Pick(blob.size() + 1)), chunk.begin(), chunk.end());
                    }
                    break;
                default:
                {
                    std::vector<uint8> const& other = _corpus[Pick(_corpus.size())].Bytes;
                    blob.resize(Pick(blob.size() + 1));
                    blob.insert(blob.end(), other.begin() + At(Pick(other.size() + 1)), other.end());
                    break;
                }
            }
        }

        std::mt19937_64 _random;
        std::vector<ObjectFuzzCorpus::Seed> const& _corpus;
        std::vector<uint32> _hashes;
    };
}

TEST(DecoderFuzzTest, SeededMutationsNeverCrashAndDecodedObjectsRoundTrip)
{
    std::string error;
    TypeCatalogPtr const catalog = ObjectFuzzCorpus::LoadCatalog(error);
    ASSERT_TRUE(catalog) << error;
    std::vector<ObjectFuzzCorpus::Seed> const corpus = ObjectFuzzCorpus::MakeSeeds(catalog);
    ASSERT_EQ(corpus.size(), 18u);
    for (ObjectFuzzCorpus::Seed const& seed : corpus)
    {
        DecodeResult const decoded = ObjectFuzzCorpus::Decode(catalog, seed.Mode, seed.Bytes);
        ASSERT_TRUE(decoded.Ok() && decoded.Object) << "a seed with mode " << int{ seed.Mode } << " does not decode: " << decoded.Detail;
    }
    std::vector<uint32> hashes;
    for (ClassInfo const* type : catalog->GetClasses())
        hashes.push_back(type->Hash);

    std::optional<std::string> const configured = Ambrose::GetEnv("AMBROSE_FUZZ_ITERATIONS");
    std::optional<uint64> const requested = configured ? Ambrose::StringTo<uint64>(*configured) : std::nullopt;
    uint64 const iterations = requested.value_or(DefaultIterations);
    SerializerLimits const limits = *ObjectFuzzCorpus::MakeOptions(0).Limits;
    std::size_t const largestCap = std::max({ std::size_t{ limits.MaxContainerCount } * sizeof(PropertyValue), std::size_t{ 65536 } * 2 + 64, limits.MaxInflatedSize }) + 4096;
    std::size_t const totalCap = limits.MaxDecodedBytes * 2 + limits.MaxInflatedSize * 2 + (std::size_t{ 1 } << 18);

    Mutator mutator(0xA5B0A5E5u, corpus, hashes);
    uint64 accepted = 0;
    std::size_t largest = 0;
    std::size_t largestTotal = 0;
    for (uint64 iteration = 0; iteration < iterations; ++iteration)
    {
        ObjectFuzzCorpus::Seed const input = mutator.Next();
        DecodeResult decoded;
        {
            AllocationScope allocations;
            decoded = ObjectFuzzCorpus::Decode(catalog, input.Mode, input.Bytes);
            largest = std::max(largest, allocations.GetLargest());
            largestTotal = std::max(largestTotal, allocations.GetTotal());
        }
        std::string const problem = ObjectFuzzCorpus::CheckDecoded(catalog, input.Mode, input.Bytes, decoded);
        ASSERT_TRUE(problem.empty()) << "iteration " << iteration << " with mode " << int{ input.Mode } << ": " << problem;
        if (decoded.Ok())
            ++accepted;
    }
    if (AllocationScope::IsSupported())
    {
        EXPECT_LE(largest, largestCap);
        EXPECT_LE(largestTotal, totalCap);
    }
    EXPECT_GT(accepted, iterations / 50);
    std::cout << "[ FUZZ     ] " << iterations << " mutations, " << accepted << " decoded, largest allocation " << largest << " bytes, most allocated by one decode " << largestTotal << " bytes" << std::endl;
}

TEST(DecoderFuzzTest, ABlobThatWouldOutgrowTheBudgetIsRefusedBeforeItAllocates)
{
    std::string error;
    TypeCatalogPtr const catalog = ObjectFuzzCorpus::LoadCatalog(error);
    ASSERT_TRUE(catalog) << error;
    PropertyObjectPtr leaf = PropertyObject::Create(catalog, "class FuzzLeaf");
    ASSERT_TRUE(leaf);
    ASSERT_EQ(leaf->Set("m_flags", PropertyValue::List(60000, PropertyValue(true))), PropertySetResult::Ok);
    SerializerOptions unlimited;
    unlimited.Limits.emplace().MaxContainerCount = 1u << 20;
    EncodeResult const encoded = ObjectSerializer::Encode(leaf.get(), unlimited);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    ASSERT_LT(encoded.Bytes.size(), 8000u);

    SerializerOptions budgeted = unlimited;
    budgeted.Limits->MaxDecodedBytes = std::size_t{ 1 } << 16;
    DecodeResult refused;
    std::size_t total = 0;
    {
        AllocationScope allocations;
        refused = ObjectSerializer::Decode(catalog, encoded.Bytes, budgeted);
        total = allocations.GetTotal();
    }
    EXPECT_EQ(refused.Status, SerializerStatus::BudgetExceeded);
    if (AllocationScope::IsSupported())
    {
        EXPECT_LE(total, budgeted.Limits->MaxDecodedBytes);
    }
    EXPECT_TRUE(ObjectSerializer::Decode(catalog, encoded.Bytes, unlimited).Ok());
}

TEST(DecoderFuzzTest, AHugeListCountInATinyBlobIsRefusedBeforeAllocating)
{
    std::string error;
    TypeCatalogPtr const catalog = ObjectFuzzCorpus::LoadCatalog(error);
    ASSERT_TRUE(catalog) << error;
    std::vector<uint8> blob;
    for (uint32 const value : { StringHash::KiStringHash("class FuzzTree"), uint32{ 0x7FFFFFFF } })
        for (int shift = 0; shift < 32; shift += 8)
            blob.push_back(static_cast<uint8>(value >> shift));
    blob.push_back(0);
    blob.push_back(0);
    ASSERT_EQ(blob.size(), 10u);

    SerializerOptions unlimited;
    unlimited.Limits.emplace().MaxContainerCount = 0xFFFFFFFFu;
    DecodeResult decoded;
    std::size_t largest = 0;
    {
        AllocationScope allocations;
        decoded = ObjectSerializer::Decode(catalog, blob, unlimited);
        largest = allocations.GetLargest();
    }
    EXPECT_EQ(decoded.Status, SerializerStatus::Truncated);
    EXPECT_EQ(decoded.Detail, "class FuzzTree.m_children lists 2147483647 elements, more than the 2 bytes left can hold");
    if (AllocationScope::IsSupported())
    {
        EXPECT_LT(largest, 4096u);
    }
}
