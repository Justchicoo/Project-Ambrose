/*
 * Project Ambrose by Imjustchico
 * Tests FileBinary table dictionaries, record values, message headers and the retail _TableList prefix.
 */

#include "BinaryTableFile.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
    BinaryTableFile TableList()
    {
        BinaryTableFile file;
        BinaryTableFile::Table table;
        table.Name = "_TableList";
        table.Fields = { { "Name", DmlType::Str, 0x28 }, { "_TargetTable", DmlType::Str, 0x28 } };
        table.Records = {
            { std::string("Base") },
            { std::string("About") },
            { std::string("PatchClient") }
        };
        file.Tables.push_back(std::move(table));
        return file;
    }
}

TEST(BinaryTableFileTest, SyntheticTableRoundTrips)
{
    BinaryTableFile source = TableList();
    source.Tables.push_back(BinaryTableFile::Table{
        "About",
        { { "Version", DmlType::Uint, 0x28 }, { "_TargetTable", DmlType::Str, 0x28 } },
        { { uint32(1) } }
    });
    source.Tables.push_back(BinaryTableFile::Table{
        "Base",
        { { "SrcFileName", DmlType::Str, 0x28 }, { "Size", DmlType::Uint, 0x28 }, { "_TargetTable", DmlType::Str, 0x28 } },
        {
            { std::string("Bin/a.dll"), uint32(12) },
            { std::string("Bin/b.dll"), uint32(34) }
        }
    });
    std::vector<uint8> const bytes = source.Write();
    BinaryTableFile const decoded = BinaryTableFile::Read(bytes);
    ASSERT_EQ(decoded.Tables.size(), source.Tables.size());
    for (std::size_t tableIndex = 0; tableIndex < source.Tables.size(); ++tableIndex)
    {
        BinaryTableFile::Table const& expected = source.Tables[tableIndex];
        BinaryTableFile::Table const& actual = decoded.Tables[tableIndex];
        EXPECT_EQ(actual.Name, expected.Name);
        ASSERT_EQ(actual.Fields.size(), expected.Fields.size());
        ASSERT_EQ(actual.Records.size(), expected.Records.size());
        for (std::size_t field = 0; field < expected.Fields.size(); ++field)
        {
            EXPECT_EQ(actual.Fields[field].Name, expected.Fields[field].Name);
            EXPECT_EQ(actual.Fields[field].Type, expected.Fields[field].Type);
            EXPECT_EQ(actual.Fields[field].Flag, expected.Fields[field].Flag);
        }
        for (std::size_t row = 0; row < expected.Records.size(); ++row)
        {
            std::size_t recordField = 0;
            for (BinaryTableFile::Field const& field : expected.Fields)
                if (field.Name != "_TargetTable")
                {
                    if (field.Type == DmlType::Str)
                        EXPECT_EQ(std::get<std::string>(actual.Records[row][recordField]), std::get<std::string>(expected.Records[row][recordField]));
                    else
                        EXPECT_EQ(std::get<uint32>(actual.Records[row][recordField]), std::get<uint32>(expected.Records[row][recordField]));
                    ++recordField;
                }
        }
    }
}

std::optional<std::string> ReferencePath()
{
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, "AMBROSE_REFERENCE_LIST") != 0 || value == nullptr)
        return std::nullopt;
    std::string path(value, size > 0 ? size - 1 : 0);
    std::free(value);
    return path;
#else
    char const* const value = std::getenv("AMBROSE_REFERENCE_LIST");
    return value == nullptr || *value == '\0' ? std::nullopt : std::optional<std::string>(value);
#endif
}

TEST(BinaryTableFileTest, ReferenceListRoundTripsWhenConfigured)
{
    std::optional<std::string> const referencePath = ReferencePath();
    if (!referencePath)
        GTEST_SKIP() << "AMBROSE_REFERENCE_LIST is not set";

    std::ifstream input(*referencePath, std::ios::binary);
    ASSERT_TRUE(input) << "Could not open " << *referencePath;
    std::vector<uint8> const original(
        (std::istreambuf_iterator<char>(input)),
        (std::istreambuf_iterator<char>()));
    ASSERT_EQ(original.size(), 1144891u);

    BinaryTableFile const decoded = BinaryTableFile::Read(original);
    EXPECT_EQ(decoded.Tables.size(), 3592u);
    EXPECT_EQ(decoded.Write(), original);
}

TEST(BinaryTableFileTest, TableListPrefixMatchesRetailDictionary)
{
    std::vector<uint8> const bytes = TableList().Write();
    std::vector<uint8> const expected{
        0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x28, 0x00,
        0x04, 0x00, 'N', 'a', 'm', 'e', 0x09, 0x28,
        0x0C, 0x00, '_', 'T', 'a', 'r', 'g', 'e', 't', 'T', 'a', 'b', 'l', 'e', 0x09, 0x28,
        0x0A, 0x00, '_', 'T', 'a', 'b', 'l', 'e', 'L', 'i', 's', 't'
    };
    ASSERT_LE(expected.size(), bytes.size());
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), bytes.begin()));
}
