/*
 * Project Ambrose by Imjustchico
 * Encodes and decodes the LatestFileList manifest in the binary and XML flavours that patch manifests round-trip cleanly.
 */

#include "LatestFileList.h"

#include <algorithm>
#include <stdexcept>

namespace
{
    BinaryTableFile::Table BuildTableList(std::vector<std::string> const& names)
    {
        BinaryTableFile::Table table;
        table.Name = "_TableList";
        table.Fields = {
            { "Name", DmlType::Str, 0x28 },
            { "_TargetTable", DmlType::Str, 0x28 }
        };
        for (std::string const& name : names)
            table.Records.push_back({ name });
        return table;
    }

    BinaryTableFile::Table BuildAboutTable(uint32 version)
    {
        BinaryTableFile::Table table;
        table.Name = "About";
        table.Fields = {
            { "Version", DmlType::Uint, 0x28 },
            { "_TargetTable", DmlType::Str, 0x28 }
        };
        table.Records.push_back({ version });
        return table;
    }

    BinaryTableFile::Table BuildPackageTable(LatestFileList::Package const& package)
    {
        BinaryTableFile::Table table;
        table.Name = package.Name;
        table.Fields = {
            { "SrcFileName", DmlType::Str, 0x28 },
            { "TarFileName", DmlType::Str, 0x28 },
            { "FileType", DmlType::Uint, 0x28 },
            { "Size", DmlType::Uint, 0x28 },
            { "HeaderSize", DmlType::Uint, 0x28 },
            { "CompressedHeaderSize", DmlType::Uint, 0x28 },
            { "CRC", DmlType::Uint, 0x28 },
            { "HeaderCRC", DmlType::Uint, 0x28 },
            { "_TargetTable", DmlType::Str, 0x28 }
        };
        for (LatestFileList::FileRecord const& record : package.Records)
        {
            table.Records.push_back({
                std::string(record.SrcFileName),
                std::string(record.TarFileName),
                record.FileType,
                record.Size,
                record.HeaderSize,
                record.CompressedHeaderSize,
                record.CRC,
                record.HeaderCRC
            });
        }
        return table;
    }

    LatestFileList::FileRecord ReadFileRecord(BinaryTableFile::Table const& table, BinaryTableFile::Record const& record)
    {
        LatestFileList::FileRecord result;
        std::size_t recordIndex = 0;
        for (BinaryTableFile::Field const& field : table.Fields)
        {
            if (field.Name == "_TargetTable")
                continue;

            if (recordIndex >= record.size())
                break;

            if (field.Name == "SrcFileName")
                result.SrcFileName = std::get<std::string>(record[recordIndex]);
            else if (field.Name == "TarFileName")
                result.TarFileName = std::get<std::string>(record[recordIndex]);
            else if (field.Name == "FileType")
                result.FileType = std::get<uint32>(record[recordIndex]);
            else if (field.Name == "Size")
                result.Size = std::get<uint32>(record[recordIndex]);
            else if (field.Name == "HeaderSize")
                result.HeaderSize = std::get<uint32>(record[recordIndex]);
            else if (field.Name == "CompressedHeaderSize")
                result.CompressedHeaderSize = std::get<uint32>(record[recordIndex]);
            else if (field.Name == "CRC")
                result.CRC = std::get<uint32>(record[recordIndex]);
            else if (field.Name == "HeaderCRC")
                result.HeaderCRC = std::get<uint32>(record[recordIndex]);
            ++recordIndex;
        }
        return result;
    }
}

std::vector<std::string> LatestFileList::TableList() const
{
    if (!TableOrder.empty())
        return TableOrder;

    std::vector<std::string> names;
    names.reserve(Packages.size() + 1);
    for (Package const& package : Packages)
        names.push_back(package.Name);
    names.push_back("About");
    return names;
}

LatestFileList LatestFileList::ReadBinary(std::span<uint8 const> bytes)
{
    BinaryTableFile decoded = BinaryTableFile::Read(bytes);
    LatestFileList manifest;
    for (BinaryTableFile::Table const& table : decoded.Tables)
    {
        if (table.Name == "_TableList")
        {
            for (BinaryTableFile::Record const& record : table.Records)
            {
                if (record.empty())
                    throw std::invalid_argument("LatestFileList binary table list contains an empty record");
                manifest.TableOrder.push_back(std::get<std::string>(record.front()));
            }
            continue;
        }
        if (table.Name == "About")
        {
            if (!table.Records.empty())
            {
                BinaryTableFile::Record const& record = table.Records.front();
                std::size_t recordIndex = 0;
                for (std::size_t i = 0; i < table.Fields.size(); ++i)
                {
                    if (table.Fields[i].Name == "Version")
                    {
                        if (recordIndex >= record.size())
                            break;
                        manifest.About.Version = std::get<uint32>(record[recordIndex]);
                        break;
                    }
                    if (table.Fields[i].Name != "_TargetTable")
                        ++recordIndex;
                }
            }
            continue;
        }

        Package package;
        package.Name = table.Name;
        for (BinaryTableFile::Record const& record : table.Records)
            package.Records.push_back(ReadFileRecord(table, record));
        manifest.Packages.push_back(std::move(package));
    }
    return manifest;
}

std::vector<uint8> LatestFileList::WriteBinary() const
{
    BinaryTableFile file;
    file.Tables.push_back(BuildTableList(TableList()));
    for (std::string const& name : TableList())
    {
        if (name == "About")
        {
            file.Tables.push_back(BuildAboutTable(About.Version));
            continue;
        }

        auto const package = std::find_if(Packages.begin(), Packages.end(), [&name](Package const& candidate) { return candidate.Name == name; });
        if (package == Packages.end())
            throw std::invalid_argument("LatestFileList table order names a missing package");
        file.Tables.push_back(BuildPackageTable(*package));
    }
    return file.Write();
}
