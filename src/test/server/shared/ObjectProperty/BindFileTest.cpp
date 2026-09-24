/*
 * Project Ambrose by Imjustchico
 * Tests BINd files on classes the test invents: the header bytes of plain and compressed files, both read back to the object written with the file's own flags, dirty-encoded properties at their default left out unless forced, a file whose root class is unknown refused while naming the hash, issues passed through, and files that are not BINd, end inside their header, carry unknown flags, would inflate past the limit, hold a corrupt zlib stream or hold no object refused with their status; also sweeps a synthetic archive of good, unknown-class, unknown-property, broken and non-BINd entries on several threads and checks the report is complete, grouped and in entry order.
 */

#include "BindFile.h"
#include "BindSweep.h"
#include "Compression.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;
    constexpr uint32 DirtySaved = Saved | 256;
    constexpr uint32 UnknownHash = 0x0F0F0F0Fu;
    constexpr uint32 NoteHash = StringHash::PropertyHash("std::string", "m_note");

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", uint32 flags = Saved)
    {
        bool const pointer = type.ends_with('*');
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json bases, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
        };
        add("class PropertyClass", Json::array(), Json::object());
        add("enum FileKind", Json::array(), Json::object());
        Json entry = Json::object();
        entry["m_filename"] = Property("std::string", "m_filename", 0);
        entry["m_id"] = Property("unsigned int", "m_id", 1);
        Json kind = Property("enum FileKind", "m_kind", 2);
        kind["enum_options"] = Json{ { "KIND_TEMPLATE", 0 }, { "KIND_ZONE", 3 } };
        entry["m_kind"] = kind;
        entry["m_child"] = Property("class FileEntry*", "m_child", 3);
        entry["m_note"] = Property("std::string", "m_note", 4, "Static", DirtySaved);
        add("class FileEntry", Json::array({ "PropertyClass" }), entry);
        Json manifest = Json::object();
        manifest["m_entries"] = Property("class FileEntry", "m_entries", 0, "Vector");
        add("class FileManifest", Json::array({ "PropertyClass" }), manifest);
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "bind.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    uint32 ReadU32(std::vector<uint8> const& bytes, std::size_t at)
    {
        return uint32{ bytes[at] } | (uint32{ bytes[at + 1] } << 8) | (uint32{ bytes[at + 2] } << 16) | (uint32{ bytes[at + 3] } << 24);
    }

    void PutU32(std::vector<uint8>& bytes, std::size_t at, uint32 value)
    {
        for (std::size_t index = 0; index < 4; ++index)
            bytes[at + index] = static_cast<uint8>(value >> (8 * index));
    }

    class BindFileTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
            _manifest = PropertyObject::Create(_catalog, "class FileManifest");
            ASSERT_TRUE(_manifest);
            PropertyValue::List entries;
            for (uint32 id = 1; id <= 300; ++id)
            {
                PropertyObjectPtr item = PropertyObject::Create(_catalog, "class FileEntry");
                ASSERT_TRUE(item);
                ASSERT_EQ(item->Set("m_filename", "ObjectData/Item" + std::to_string(id) + ".xml"), PropertySetResult::Ok);
                ASSERT_EQ(item->Set("m_id", id), PropertySetResult::Ok);
                ASSERT_EQ(item->Set("m_kind", int64{ id % 2 == 0 ? 3 : 0 }), PropertySetResult::Ok);
                if (id % 3 == 0)
                {
                    ASSERT_EQ(item->Set("m_note", "noted"), PropertySetResult::Ok);
                }
                entries.emplace_back(std::move(item));
            }
            ASSERT_EQ(_manifest->Set("m_entries", std::move(entries)), PropertySetResult::Ok);
        }

        TypeCatalogPtr _catalog;
        PropertyObjectPtr _manifest;
    };
}

TEST_F(BindFileTest, PlainAndCompressedFilesCarryTheirHeaderAndReadBack)
{
    EncodeResult const plain = BindFile::Write(_manifest.get());
    ASSERT_TRUE(plain.Ok()) << plain.Detail;
    ASSERT_GT(plain.Bytes.size(), 12u);
    EXPECT_TRUE(std::equal(BindFile::Magic.begin(), BindFile::Magic.end(), plain.Bytes.begin()));
    EXPECT_EQ(ReadU32(plain.Bytes, 4), 7u);
    EXPECT_EQ(ReadU32(plain.Bytes, 8), StringHash::KiStringHash("class FileManifest"));

    EncodeResult const packed = BindFile::Write(_manifest.get(), BindFile::DefaultFlags | SerializerFlag::Compress);
    ASSERT_TRUE(packed.Ok()) << packed.Detail;
    EXPECT_EQ(ReadU32(packed.Bytes, 4), 15u);
    EXPECT_EQ(packed.Bytes[8], 0x01);
    EXPECT_EQ(ReadU32(packed.Bytes, 9), plain.Bytes.size() - BindFile::HeaderSize);
    EXPECT_EQ(packed.Bytes[13], 0x78);
    EXPECT_LT(packed.Bytes.size(), plain.Bytes.size());

    for (EncodeResult const* file : { &plain, &packed })
    {
        BindReadResult const read = BindFile::Read(_catalog, file->Bytes);
        ASSERT_TRUE(read.Ok()) << read.Detail;
        EXPECT_EQ(read.RootClassHash, StringHash::KiStringHash("class FileManifest"));
        EXPECT_TRUE(read.Decoded.Issues.empty());
        ASSERT_TRUE(read.Decoded.Object);
        EXPECT_TRUE(*read.Decoded.Object == *_manifest);
    }

    EncodeResult const numeric = BindFile::Write(_manifest.get(), SerializerFlag::SerializeFlags);
    ASSERT_TRUE(numeric.Ok()) << numeric.Detail;
    EXPECT_EQ(ReadU32(numeric.Bytes, 4), 1u);
    BindReadResult const numericRead = BindFile::Read(_catalog, numeric.Bytes);
    ASSERT_TRUE(numericRead.Ok()) << numericRead.Detail;
    EXPECT_TRUE(*numericRead.Decoded.Object == *_manifest);
}

TEST_F(BindFileTest, DirtyEncodedPropertiesAtTheirDefaultAreLeftOutUnlessForced)
{
    auto const count = [](std::vector<uint8> const& bytes)
    {
        std::size_t found = 0;
        for (std::size_t at = 0; at + 4 <= bytes.size(); ++at)
            found += ReadU32(bytes, at) == NoteHash ? 1 : 0;
        return found;
    };
    EncodeResult const written = BindFile::Write(_manifest.get());
    ASSERT_TRUE(written.Ok()) << written.Detail;
    EXPECT_EQ(count(written.Bytes), 100u);
    EncodeResult const forced = BindFile::Write(_manifest.get(), BindFile::DefaultFlags | SerializerFlag::ForceDirtyEncode);
    ASSERT_TRUE(forced.Ok()) << forced.Detail;
    EXPECT_EQ(count(forced.Bytes), 300u);
    EXPECT_EQ(ReadU32(forced.Bytes, 4), 23u);
    for (EncodeResult const* file : { &written, &forced })
    {
        BindReadResult const read = BindFile::Read(_catalog, file->Bytes);
        ASSERT_TRUE(read.Ok()) << read.Detail;
        EXPECT_TRUE(*read.Decoded.Object == *_manifest);
    }
}

TEST_F(BindFileTest, BrokenFilesAreRefusedWithTheirStatus)
{
    EncodeResult const packed = BindFile::Write(_manifest.get(), BindFile::DefaultFlags | SerializerFlag::Compress);
    ASSERT_TRUE(packed.Ok()) << packed.Detail;

    std::vector<uint8> const text{ '<', '?', 'x', 'm', 'l' };
    EXPECT_EQ(BindFile::Read(_catalog, text).Status, BindStatus::NotBind);
    EXPECT_FALSE(BindFile::IsBind(text));
    std::vector<uint8> const shortHeader(packed.Bytes.begin(), packed.Bytes.begin() + 6);
    EXPECT_EQ(BindFile::Read(_catalog, shortHeader).Status, BindStatus::Truncated);
    std::vector<uint8> const shortCompressed(packed.Bytes.begin(), packed.Bytes.begin() + 11);
    BindReadResult const cut = BindFile::Read(_catalog, shortCompressed);
    EXPECT_EQ(cut.Status, BindStatus::Truncated);
    EXPECT_EQ(cut.Detail, "is 11 bytes, shorter than the 13-byte header of a compressed BINd file");

    std::vector<uint8> strange = packed.Bytes;
    PutU32(strange, 4, 15u | 0x100u);
    BindReadResult const unknown = BindFile::Read(_catalog, strange);
    EXPECT_EQ(unknown.Status, BindStatus::UnknownFlags);
    EXPECT_EQ(unknown.Detail, "carries serializer flags 0x10f, which include bits no known mode uses");

    std::vector<uint8> huge = packed.Bytes;
    PutU32(huge, 9, 0x7FFFFFFFu);
    BindReadResult const tooLarge = BindFile::Read(_catalog, huge);
    EXPECT_EQ(tooLarge.Status, BindStatus::TooLarge);
    SerializerLimits tight = BindFile::GetDefaultLimits();
    tight.MaxInflatedSize = 1024;
    EXPECT_EQ(BindFile::Read(_catalog, packed.Bytes, tight).Status, BindStatus::TooLarge);

    std::vector<uint8> wrongSize = packed.Bytes;
    PutU32(wrongSize, 9, ReadU32(packed.Bytes, 9) + 1);
    EXPECT_EQ(BindFile::Read(_catalog, wrongSize).Status, BindStatus::BadCompression);
    std::vector<uint8> corrupt = packed.Bytes;
    for (std::size_t index = 20; index < corrupt.size() && index < 60; ++index)
        corrupt[index] = static_cast<uint8>(corrupt[index] ^ 0xA5);
    EXPECT_EQ(BindFile::Read(_catalog, corrupt).Status, BindStatus::BadCompression);

    EncodeResult const plain = BindFile::Write(_manifest.get());
    ASSERT_TRUE(plain.Ok()) << plain.Detail;
    std::vector<uint8> stranger = plain.Bytes;
    PutU32(stranger, 8, UnknownHash);
    BindReadResult const refused = BindFile::Read(_catalog, stranger);
    EXPECT_EQ(refused.Status, BindStatus::DecodeFailed);
    EXPECT_EQ(refused.Decoded.Status, SerializerStatus::UnknownClass);
    EXPECT_EQ(refused.RootClassHash, UnknownHash);
    EXPECT_FALSE(refused.Decoded.Object);
    EXPECT_EQ(BindFile::GetStatusName(refused.Status), "the file's object could not be decoded");

    EXPECT_EQ(BindFile::Write(_manifest.get(), static_cast<SerializerFlag>(0x40)).Status, SerializerStatus::UnsupportedFlags);

    std::vector<uint8> const empty{ 'B', 'I', 'N', 'd', 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    BindReadResult const nothing = BindFile::Read(_catalog, empty);
    EXPECT_EQ(nothing.Status, BindStatus::DecodeFailed);
    EXPECT_EQ(nothing.Decoded.Status, SerializerStatus::NullNotAllowed);
    EXPECT_FALSE(nothing.Decoded.Object);
}

TEST_F(BindFileTest, ASweepReportsEveryFileInEntryOrderOnAnyThreadCount)
{
    EncodeResult const plain = BindFile::Write(_manifest.get());
    EncodeResult const packed = BindFile::Write(_manifest.get(), BindFile::DefaultFlags | SerializerFlag::Compress);
    ASSERT_TRUE(plain.Ok() && packed.Ok());

    PropertyObjectPtr const withChild = PropertyObject::Create(_catalog, "class FileEntry");
    ASSERT_TRUE(withChild);
    ASSERT_EQ(withChild->Set("m_child", PropertyObject::Create(_catalog, "class FileEntry")), PropertySetResult::Ok);
    EncodeResult childFile = BindFile::Write(withChild.get());
    ASSERT_TRUE(childFile.Ok()) << childFile.Detail;
    std::vector<uint8> nestedStranger = childFile.Bytes;
    uint32 const childHash = StringHash::KiStringHash("class FileEntry");
    for (std::size_t at = 16; at + 4 <= nestedStranger.size(); ++at)
    {
        if (ReadU32(nestedStranger, at) == childHash)
        {
            PutU32(nestedStranger, at, UnknownHash);
            break;
        }
    }
    std::vector<uint8> rootStranger = plain.Bytes;
    PutU32(rootStranger, 8, UnknownHash);
    std::vector<uint8> broken = plain.Bytes;
    PutU32(broken, 12, 0);
    std::vector<uint8> oddProperty = plain.Bytes;
    uint32 const idHash = StringHash::PropertyHash("unsigned int", "m_id");
    for (std::size_t at = 16, replaced = 0; at + 4 <= oddProperty.size() && replaced < 2; ++at)
    {
        if (ReadU32(oddProperty, at) == idHash)
        {
            PutU32(oddProperty, at, UnknownHash);
            ++replaced;
        }
    }

    KiwadBuilder builder(2);
    for (int copy = 0; copy < 20; ++copy)
    {
        builder.Add("Plain" + std::to_string(copy) + ".xml", plain.Bytes, copy % 2 == 0);
        builder.Add("Packed" + std::to_string(copy) + ".xml", packed.Bytes, false);
        builder.Add("Notes" + std::to_string(copy) + ".txt", "not a BINd file", true);
    }
    builder.Add("Nested.xml", nestedStranger, true);
    builder.Add("Root.xml", rootStranger, false);
    builder.Add("Broken.xml", broken, false);
    builder.Add("OddA.xml", oddProperty, true);
    builder.Add("OddB.xml", oddProperty, false);
    LogTestDirectory directory;
    std::filesystem::path const path = directory.Path() / "Sweep.wad";
    std::vector<uint8> const archiveBytes = builder.Build();
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(archiveBytes.data()), static_cast<std::streamsize>(archiveBytes.size()));
    }
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error);
    ASSERT_TRUE(archive) << error;

    for (unsigned const threads : { 1u, 3u, 16u })
    {
        BindSweepReport const report = BindSweep::Run(*archive, _catalog, threads);
        EXPECT_EQ(report.Entries, 65u) << threads;
        EXPECT_EQ(report.Files, 45u) << threads;
        EXPECT_EQ(report.Decoded, 43u) << threads;
        EXPECT_EQ(report.ReadErrors, 0u) << threads;
        ASSERT_EQ(report.Failures.size(), 2u) << threads;
        EXPECT_EQ(report.Failures[0].File, "Root.xml");
        EXPECT_EQ(report.Failures[0].DecodeStatus, SerializerStatus::UnknownClass);
        EXPECT_EQ(report.Failures[0].RootClassHash, UnknownHash);
        EXPECT_EQ(report.Failures[1].File, "Broken.xml");
        EXPECT_EQ(report.Failures[1].DecodeStatus, SerializerStatus::BadSize);
        ASSERT_EQ(report.UnknownClasses.size(), 1u) << threads;
        EXPECT_EQ(report.UnknownClasses[0].Hash, UnknownHash);
        EXPECT_EQ(report.UnknownClasses[0].Count, 2u);
        EXPECT_EQ(report.UnknownClasses[0].Files, 2u);
        EXPECT_EQ(report.UnknownClasses[0].FirstFile, "Nested.xml");
        EXPECT_EQ(report.UnknownClasses[0].FirstPath, "class FileEntry.m_child");
        ASSERT_EQ(report.Issues.size(), 1u) << threads;
        EXPECT_EQ(report.Issues[0].Kind, DecodeIssueKind::UnknownProperty);
        EXPECT_EQ(report.Issues[0].Hash, UnknownHash);
        EXPECT_EQ(report.Issues[0].Count, 4u);
        EXPECT_EQ(report.Issues[0].Files, 2u);
        EXPECT_EQ(report.Issues[0].FirstFile, "OddA.xml");
        EXPECT_EQ(report.Issues[0].FirstPath, "class FileManifest.m_entries[0]");
        ASSERT_EQ(report.Issues[0].BitSizes.size(), 1u);
        EXPECT_EQ(report.Issues[0].BitSizes.at(32), 4u);
    }
}
