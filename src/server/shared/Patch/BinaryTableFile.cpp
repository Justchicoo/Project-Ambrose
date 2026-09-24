/*
 * Project Ambrose by Imjustchico
 * Encodes and decodes FileBinary table dictionaries and records with the client's service/order message headers and DML field layout.
 */

#include "BinaryTableFile.h"

#include <fmt/format.h>

#include <limits>
#include <stdexcept>

namespace
{
    constexpr uint8 DictService = 2;
    constexpr uint8 DictOrder = 1;
    constexpr uint8 RecordOrder = 2;

    bool IsRecordField(BinaryTableFile::Field const& field)
    {
        return field.Name != "_TargetTable";
    }

    uint8 TypeCode(DmlType type)
    {
        switch (type)
        {
            case DmlType::Uint: return 3;
            case DmlType::Str: return 9;
            default: throw std::invalid_argument("FileBinary supports only UINT and STR fields");
        }
    }

    DmlType TypeFromCode(uint8 code)
    {
        if (code == 3)
            return DmlType::Uint;
        if (code == 9)
            return DmlType::Str;
        throw std::invalid_argument(fmt::format("FileBinary has unsupported field type {}", code));
    }

    void WriteMessageHeader(ByteBuffer& output, uint8 order, std::size_t bodySize)
    {
        if (bodySize > std::numeric_limits<uint16>::max() - 4)
            throw std::length_error("FileBinary message body exceeds its u16 length");
        output.Write(DictService);
        output.Write(order);
        output.Write(static_cast<uint16>(bodySize + 4));
    }

    ByteBuffer MessageBody(std::span<uint8 const> bytes)
    {
        if (bytes.size() < 4)
            throw std::invalid_argument("FileBinary message is shorter than its header");
        ByteBuffer header(bytes);
        uint8 const service = header.Read<uint8>();
        uint8 const order = header.Read<uint8>();
        uint16 const length = header.Read<uint16>();
        if (service != DictService || (order != DictOrder && order != RecordOrder) || length != bytes.size())
            throw std::invalid_argument("FileBinary message header is invalid");
        return ByteBuffer(bytes.subspan(4));
    }
}

BinaryTableFile BinaryTableFile::Read(std::span<uint8 const> bytes)
{
    ByteBuffer input(bytes);
    BinaryTableFile file;
    while (input.GetRemaining() != 0)
    {
        uint32 const recordCount = input.Read<uint32>();
        uint8 const service = input.Read<uint8>();
        uint8 const order = input.Read<uint8>();
        uint16 const dictLength = input.Read<uint16>();
        if (service != DictService || order != DictOrder || dictLength < 4 || static_cast<std::size_t>(dictLength - 4) > input.GetRemaining())
            throw std::invalid_argument("FileBinary dictionary header is invalid");
        std::vector<uint8> dictBytes;
        dictBytes.reserve(dictLength);
        dictBytes.push_back(service);
        dictBytes.push_back(order);
        dictBytes.push_back(static_cast<uint8>(dictLength));
        dictBytes.push_back(static_cast<uint8>(dictLength >> 8));
        std::span<uint8 const> const body = input.ReadBytes(dictLength - 4);
        dictBytes.insert(dictBytes.end(), body.begin(), body.end());
        ByteBuffer dict = MessageBody(dictBytes);

        Table table;
        while (dict.GetRemaining() > 2)
        {
            std::size_t const fieldStart = dict.GetReadPosition();
            uint16 const nameLength = dict.Read<uint16>();
            if (dict.GetRemaining() < static_cast<std::size_t>(nameLength) + 2)
            {
                dict.SetReadPosition(fieldStart);
                break;
            }
            std::string name(reinterpret_cast<char const*>(dict.ReadBytes(nameLength).data()), nameLength);
            uint8 const typeCode = dict.Read<uint8>();
            uint8 const flag = dict.Read<uint8>();
            if (typeCode != 3 && typeCode != 9)
            {
                dict.SetReadPosition(fieldStart);
                break;
            }
            table.Fields.push_back(Field{ std::move(name), TypeFromCode(typeCode), flag });
        }
        table.Name = Dml::ReadStr(dict);
        if (dict.GetRemaining() != 0)
            throw std::invalid_argument("FileBinary dictionary has trailing bytes");

        for (uint32 row = 0; row < recordCount; ++row)
        {
            uint8 const recordService = input.Read<uint8>();
            uint8 const recordOrder = input.Read<uint8>();
            uint16 const recordLength = input.Read<uint16>();
            if (recordService != DictService || recordOrder != RecordOrder || recordLength < 4 || static_cast<std::size_t>(recordLength - 4) > input.GetRemaining())
                throw std::invalid_argument("FileBinary record header is invalid");
            std::span<uint8 const> const recordBody = input.ReadBytes(recordLength - 4);
            ByteBuffer record(recordBody);
            Record values;
            values.reserve(table.Fields.size());
            for (Field const& field : table.Fields)
                if (IsRecordField(field))
                    values.push_back(Dml::ReadValue(record, field.Type));
            if (record.GetRemaining() != 0)
                throw std::invalid_argument("FileBinary record has trailing bytes");
            table.Records.push_back(std::move(values));
        }
        file.Tables.push_back(std::move(table));
    }
    return file;
}

std::vector<uint8> BinaryTableFile::Write() const
{
    ByteBuffer output;
    for (Table const& table : Tables)
    {
        if (table.Records.size() > std::numeric_limits<uint32>::max())
            throw std::length_error("FileBinary table has too many records");
        ByteBuffer dict;
        for (Field const& field : table.Fields)
        {
            Dml::WriteStr(dict, field.Name);
            dict.Write(TypeCode(field.Type));
            dict.Write(field.Flag);
        }
        Dml::WriteStr(dict, table.Name);
        output.Write(static_cast<uint32>(table.Records.size()));
        WriteMessageHeader(output, DictOrder, dict.GetSize());
        output.WriteBytes(dict.GetData());
        for (Record const& record : table.Records)
        {
            std::size_t recordFieldCount = 0;
            for (Field const& field : table.Fields)
                if (IsRecordField(field))
                    ++recordFieldCount;
            if (record.size() != recordFieldCount)
                throw std::invalid_argument("FileBinary record field count does not match its dictionary");
            ByteBuffer body;
            std::size_t recordIndex = 0;
            for (std::size_t i = 0; i < table.Fields.size(); ++i)
                if (IsRecordField(table.Fields[i]))
                    Dml::WriteValue(body, table.Fields[i].Type, record[recordIndex++]);
            WriteMessageHeader(output, RecordOrder, body.GetSize());
            output.WriteBytes(body.GetData());
        }
    }
    return output.Release();
}
