/*
 * Project Ambrose by Imjustchico
 * Round-trips a default object of every property class in the r806919 type dump through the compact codec with the transmit and public masks, and decodes compact ObjectProperty samples captured on the maintainer's own machine, when AMBROSE_TYPE_DUMP_PATH names the r806919 type dump and AMBROSE_OBJECT_SAMPLES_DIR a folder of .bin files named after a property class, unwrapping those that start with an envelope, and checks each consumes exactly all its bytes and re-encodes byte for byte, including the 1256-byte BadgeFilterInfoList and the 363-byte BadgeInfoList.
 */

#include "BlobEnvelope.h"
#include "Environment.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace
{
    std::vector<uint8> ReadFile(std::filesystem::path const& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<uint8>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    uint32 LeadingHash(std::vector<uint8> const& bytes)
    {
        if (bytes.size() < 4)
            return 0;
        return uint32{ bytes[0] } | (uint32{ bytes[1] } << 8) | (uint32{ bytes[2] } << 16) | (uint32{ bytes[3] } << 24);
    }
}

TEST(CompactCodecClientTest, CapturedSamplesDecodeExactlyAndReencodeByteForByte)
{
    std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    std::optional<std::string> const samples = Ambrose::GetEnv("AMBROSE_OBJECT_SAMPLES_DIR");
    if (!dump || dump->empty() || !samples || samples->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump and AMBROSE_OBJECT_SAMPLES_DIR to a folder of captured ClassName_*.bin blobs to run this test";

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*dump)));
    TypeCatalogPtr const catalog = registry.GetCatalog();

    std::map<std::string, std::set<std::size_t>> checked;
    std::error_code error;
    for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator(LogConfig::Utf8Path(*samples), error))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".bin")
            continue;
        std::string const stem = entry.path().stem().string();
        std::size_t const separator = stem.rfind('_');
        if (separator == std::string::npos)
            continue;
        ClassInfo const* const named = catalog->FindClass("class " + stem.substr(0, separator));
        if (!named || named->Kind != ClassKind::PropertyClass)
            continue;

        std::vector<uint8> bytes = ReadFile(entry.path());
        ClassInfo const* const leading = catalog->FindClass(LeadingHash(bytes));
        if (!leading || leading->Kind != ClassKind::PropertyClass)
        {
            BlobEnvelope::UnwrapResult unwrapped = BlobEnvelope::Unwrap(bytes, 1 << 24);
            if (!unwrapped.Succeeded())
            {
                ADD_FAILURE() << stem << " starts with no property class hash and is not an envelope: " << BlobEnvelope::GetStatusName(unwrapped.Code);
                continue;
            }
            bytes = std::move(unwrapped.Data);
        }

        DecodeResult const decoded = ObjectSerializer::Decode(catalog, bytes);
        if (!decoded.Ok())
        {
            ADD_FAILURE() << stem << ": " << ObjectSerializer::GetStatusName(decoded.Status) << ": " << decoded.Detail;
            continue;
        }
        ASSERT_TRUE(decoded.Object) << stem;
        EXPECT_EQ(decoded.BytesRead, bytes.size()) << stem;

        EncodeResult const encoded = ObjectSerializer::Encode(decoded.Object.get());
        ASSERT_TRUE(encoded.Ok()) << stem << ": " << encoded.Detail;
        EXPECT_TRUE(encoded.Bytes == bytes) << stem << " re-encodes to " << encoded.Bytes.size() << " bytes instead of the " << bytes.size() << " it was read from";
        checked[decoded.Object->GetClass().Name].insert(bytes.size());
    }
    ASSERT_FALSE(error) << error.message();

    for (auto const& [name, sizes] : checked)
    {
        std::string list;
        for (std::size_t size : sizes)
            list += (list.empty() ? "" : ", ") + std::to_string(size);
        std::cout << "[ SAMPLES  ] " << name << ": " << list << " bytes\n";
    }
    EXPECT_TRUE(checked["class BadgeFilterInfoList"].contains(1256));
    EXPECT_TRUE(checked["class BadgeInfoList"].contains(363));
}

TEST(CompactCodecClientTest, EveryPropertyClassRoundTripsWithItsDefaults)
{
    std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!dump || dump->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump (format v2) from your own client to run this test";

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*dump)));
    TypeCatalogPtr const catalog = registry.GetCatalog();

    std::size_t roundTrips = 0;
    std::size_t failures = 0;
    for (ClassInfo const* type : catalog->GetClasses())
    {
        if (type->Kind != ClassKind::PropertyClass)
            continue;
        PropertyObjectPtr const object = PropertyObject::Create(catalog, *type);
        ASSERT_TRUE(object) << type->Name;
        for (uint32 const mask : { SerializerOptions::TransmitMask, SerializerOptions::PublicMask })
        {
            SerializerOptions options;
            options.Mask = mask;
            EncodeResult const encoded = ObjectSerializer::Encode(object.get(), options);
            DecodeResult const decoded = encoded.Ok() ? ObjectSerializer::Decode(catalog, encoded.Bytes, options) : DecodeResult{};
            if (!encoded.Ok() || !decoded.Ok() || !decoded.Object || !(*decoded.Object == *object))
            {
                if (++failures <= 20)
                    ADD_FAILURE() << type->Name << " with mask " << mask << ": " << encoded.Detail << decoded.Detail;
                continue;
            }
            ++roundTrips;
        }
    }
    EXPECT_EQ(failures, 0u);
    EXPECT_EQ(roundTrips, catalog->GetClassCount(ClassKind::PropertyClass) * 2);
}
