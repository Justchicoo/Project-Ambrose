/*
 * Project Ambrose by Imjustchico
 * Verifies versionable BINd objects: synthetic values decode equal after encoding, the crown hat and TemplateManifest payloads re-encode byte-exactly, and a deterministic 2000-file sample reports its first differing bit.
 */

#include "BindFile.h"
#include "Compression.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace
{
    std::optional<std::vector<uint8>> Payload(std::span<uint8 const> bytes)
    {
        if (!BindFile::IsBind(bytes) || bytes.size() < BindFile::HeaderSize)
            return std::nullopt;
        uint32 const flags = uint32{ bytes[4] } | (uint32{ bytes[5] } << 8) | (uint32{ bytes[6] } << 16) | (uint32{ bytes[7] } << 24);
        if ((flags & static_cast<uint32>(SerializerFlag::Compress)) == 0)
            return std::vector<uint8>(bytes.begin() + BindFile::HeaderSize, bytes.end());
        if (bytes.size() < BindFile::CompressedHeaderSize)
            return std::nullopt;
        uint32 const size = uint32{ bytes[9] } | (uint32{ bytes[10] } << 8) | (uint32{ bytes[11] } << 16) | (uint32{ bytes[12] } << 24);
        Ambrose::Compression::InflateResult const inflated = Ambrose::Compression::InflateExact(bytes.subspan(BindFile::CompressedHeaderSize), size, Ambrose::Compression::Format::Zlib);
        return inflated.Succeeded() ? std::optional<std::vector<uint8>>(inflated.Data) : std::nullopt;
    }

    std::string FirstDifference(std::span<uint8 const> expected, std::span<uint8 const> actual)
    {
        std::size_t const common = std::min(expected.size(), actual.size());
        for (std::size_t byte = 0; byte < common; ++byte)
            if (expected[byte] != actual[byte])
            {
                uint8 const difference = expected[byte] ^ actual[byte];
                unsigned bit = 0;
                while ((difference & (uint8{ 1 } << bit)) == 0)
                    ++bit;
                return fmt::format("bit {} (byte {} bit {}): expected {:#04x}, got {:#04x}", byte * 8 + bit, byte, bit, expected[byte], actual[byte]);
            }
        return expected.size() == actual.size() ? std::string() : fmt::format("payload length differs: expected {} bytes, got {}", expected.size(), actual.size());
    }

    class VersionableRoundTripTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH to run the client-gated BINd round-trip";
            _registry = std::make_unique<TypeRegistry>();
            ASSERT_TRUE(_registry->LoadFromFile(LogConfig::Utf8Path(*dump))) << (_registry->GetErrors().empty() ? std::string() : _registry->GetErrors().front());
            _catalog = _registry->GetCatalog();
            std::string error;
            _archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad", error);
            ASSERT_TRUE(_archive) << error;
        }

        bool Check(std::string_view name)
        {
            KiwadReadResult const source = _archive->Read(name);
            EXPECT_TRUE(source.Succeeded()) << name << ": " << source.Error;
            if (!source.Succeeded())
                return false;
            BindReadResult const decoded = BindFile::Read(_catalog, source.Data);
            EXPECT_TRUE(decoded.Ok()) << name << ": " << decoded.Detail;
            if (!decoded.Ok())
                return false;
            EncodeResult const encoded = BindFile::Write(decoded.Decoded.Object.get(), decoded.Flags);
            EXPECT_TRUE(encoded.Ok()) << name << ": " << encoded.Detail;
            if (!encoded.Ok())
                return false;
            std::optional<std::vector<uint8>> const expected = Payload(source.Data);
            std::optional<std::vector<uint8>> const actual = Payload(encoded.Bytes);
            if (!expected || !actual)
            {
                ADD_FAILURE() << name << ": could not inflate the source or encoded payload";
                return false;
            }
            EXPECT_EQ(FirstDifference(*expected, *actual), "") << name;
            return *expected == *actual;
        }

        std::unique_ptr<TypeRegistry> _registry;
        TypeCatalogPtr _catalog;
        std::unique_ptr<KiwadArchive> _archive;
    };
}

TEST_F(VersionableRoundTripTest, HatTemplateAndManifestAreByteExact)
{
    EXPECT_TRUE(Check("ObjectData/CrownItems/Series58/Hats/Crowns-S58-Hats-L110-BS-008-01.xml"));
    EXPECT_TRUE(Check("TemplateManifest.xml"));
}

TEST_F(VersionableRoundTripTest, SyntheticVersionableObjectRoundTrips)
{
    ClassInfo const* const type = _catalog->FindClass("class PropertyClass");
    ASSERT_NE(type, nullptr);
    PropertyObjectPtr const source = PropertyObject::Create(_catalog, *type);
    ASSERT_NE(source, nullptr);
    EncodeResult const encoded = ObjectSerializer::Encode(source.get(), SerializerOptions{ .Versionable = true });
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    DecodeResult const decoded = ObjectSerializer::Decode(_catalog, encoded.Bytes, SerializerOptions{ .Versionable = true });
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(*source, *decoded.Object);
}

TEST_F(VersionableRoundTripTest, TwoThousandDecodedFilesAreByteExact)
{
    std::vector<std::string> names;
    for (KiwadEntry const& entry : _archive->GetEntries())
        if (entry.Name.ends_with(".xml"))
            names.push_back(entry.Name);
    std::mt19937 generator(806919);
    std::shuffle(names.begin(), names.end(), generator);
    std::vector<std::string> decodable;
    decodable.reserve(2000);
    for (std::string const& name : names)
    {
        KiwadReadResult const source = _archive->Read(name);
        if (!source.Succeeded())
            continue;
        BindReadResult const decoded = BindFile::Read(_catalog, source.Data);
        if (decoded.Ok() && decoded.Decoded.Issues.empty())
            decodable.push_back(name);
        if (decodable.size() == 2000)
            break;
    }
    ASSERT_EQ(decodable.size(), 2000u);
    for (std::string const& name : decodable)
        EXPECT_TRUE(Check(name)) << name;
}
