/*
 * Project Ambrose by Imjustchico
 * Models the LatestFileList manifest as package records plus the table list and About metadata the client reads from XML and binary forms.
 */

#ifndef AMBROSE_LATESTFILELIST_H
#define AMBROSE_LATESTFILELIST_H

#include "BinaryTableFile.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

class LatestFileList
{
public:
    struct FileRecord
    {
        std::string SrcFileName;
        std::string TarFileName;
        uint32 FileType = 0;
        uint32 Size = 0;
        uint32 HeaderSize = 0;
        uint32 CompressedHeaderSize = 0;
        uint32 CRC = 0;
        uint32 HeaderCRC = 0;
    };

    struct Package
    {
        std::string Name;
        std::vector<FileRecord> Records;
    };

    struct About
    {
        uint32 Version = 1;
    };

    static LatestFileList ReadBinary(std::span<uint8 const> bytes);
    std::vector<uint8> WriteBinary() const;

    std::vector<std::string> TableList() const;

    About About;
    std::vector<Package> Packages;
};

#endif
