/*
 * Project Ambrose by Imjustchico
 * Reads and writes the client's FileBinary table lists, preserving table dictionaries, field flags, record order and DML values.
 */

#ifndef AMBROSE_BINARYTABLEFILE_H
#define AMBROSE_BINARYTABLEFILE_H

#include "DmlTypes.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

class BinaryTableFile
{
public:
    struct Field
    {
        std::string Name;
        DmlType Type = DmlType::Str;
        uint8 Flag = 0x28;
    };

    using Record = std::vector<DmlValue>;

    struct Table
    {
        std::string Name;
        std::vector<Field> Fields;
        std::vector<Record> Records;
    };

    static BinaryTableFile Read(std::span<uint8 const> bytes);
    std::vector<uint8> Write() const;

    std::vector<Table> Tables;
};

#endif
