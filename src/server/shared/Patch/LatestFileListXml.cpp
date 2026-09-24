/*
 * Project Ambrose by Imjustchico
 * Serializes the LatestFileList model as a tolerant XML document and reads it back without depending on a specific client installation.
 */

#include "LatestFileListXml.h"

#include <pugixml.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    class StringWriter final : public pugi::xml_writer
    {
    public:
        void write(void const* data, std::size_t size) override
        {
            Result.append(static_cast<char const*>(data), size);
        }

        std::string Result;
    };

    std::string Value(pugi::xml_node node)
    {
        auto const text = node.text();
        return text ? text.as_string() : std::string();
    }

    void SetText(pugi::xml_node node, std::string const& value)
    {
        node.text().set(value.c_str());
    }

    uint32 ParseUint(pugi::xml_node node)
    {
        std::string const content = Value(node);
        if (content.empty())
            return 0;
        return static_cast<uint32>(std::stoul(content));
    }

    void AddValue(pugi::xml_node record, std::string const& name, std::string const& value)
    {
        pugi::xml_node field = record.append_child(name.c_str());
        field.append_attribute("TYPE") = "STR";
        SetText(field, value);
    }

    void AddValue(pugi::xml_node record, std::string const& name, uint32 value)
    {
        pugi::xml_node field = record.append_child(name.c_str());
        field.append_attribute("TYPE") = "UINT";
        field.text().set(value);
    }

    LatestFileList::FileRecord ReadRecord(pugi::xml_node recordNode)
    {
        LatestFileList::FileRecord record;
        for (pugi::xml_node child = recordNode.first_child(); child; child = child.next_sibling())
        {
            std::string const name = child.name();
            if (name == "SrcFileName")
                record.SrcFileName = Value(child);
            else if (name == "TarFileName")
                record.TarFileName = Value(child);
            else if (name == "FileType")
                record.FileType = ParseUint(child);
            else if (name == "Size")
                record.Size = ParseUint(child);
            else if (name == "HeaderSize")
                record.HeaderSize = ParseUint(child);
            else if (name == "CompressedHeaderSize")
                record.CompressedHeaderSize = ParseUint(child);
            else if (name == "CRC")
                record.CRC = ParseUint(child);
            else if (name == "HeaderCRC")
                record.HeaderCRC = ParseUint(child);
        }
        return record;
    }

    void ReadPackage(pugi::xml_node packageNode, LatestFileList& list)
    {
        LatestFileList::Package package;
        if (pugi::xml_attribute const name = packageNode.attribute("Name"); name)
            package.Name = name.value();
        for (pugi::xml_node child = packageNode.first_child(); child; child = child.next_sibling())
        {
            if (std::string(child.name()) == "Name" && package.Name.empty())
                package.Name = Value(child);
            else if (std::string(child.name()) == "RECORD")
                package.Records.push_back(ReadRecord(child));
        }
        if (!package.Name.empty())
            list.Packages.push_back(std::move(package));
    }

}

std::string LatestFileListXml::Write(LatestFileList const& list)
{
    pugi::xml_document document;
    pugi::xml_node root = document.append_child("LatestFileList");

    pugi::xml_node tableList = root.append_child("_TableList");
    for (std::string const& name : list.TableList())
    {
        pugi::xml_node record = tableList.append_child("RECORD");
        pugi::xml_node field = record.append_child("Name");
        field.append_attribute("TYPE") = "STR";
        SetText(field, name);
    }

    pugi::xml_node about = root.append_child("About");
    pugi::xml_node aboutRecord = about.append_child("RECORD");
    pugi::xml_node version = aboutRecord.append_child("Version");
    version.append_attribute("TYPE") = "UINT";
    version.text().set(list.About.Version);

    for (LatestFileList::Package const& package : list.Packages)
    {
        pugi::xml_node packageNode = root.append_child("Package");
        packageNode.append_attribute("Name") = package.Name.c_str();
        for (LatestFileList::FileRecord const& record : package.Records)
        {
            pugi::xml_node entry = packageNode.append_child("RECORD");
            AddValue(entry, "SrcFileName", record.SrcFileName);
            AddValue(entry, "TarFileName", record.TarFileName);
            AddValue(entry, "FileType", record.FileType);
            AddValue(entry, "Size", record.Size);
            AddValue(entry, "HeaderSize", record.HeaderSize);
            AddValue(entry, "CompressedHeaderSize", record.CompressedHeaderSize);
            AddValue(entry, "CRC", record.CRC);
            AddValue(entry, "HeaderCRC", record.HeaderCRC);
        }
    }

    std::string xml;
    StringWriter writer;
    document.save(writer, "  ", pugi::format_default, pugi::encoding_utf8);
    xml = writer.Result;
    return xml;
}

LatestFileList LatestFileListXml::Read(std::string_view xml)
{
    LatestFileList list;
    std::vector<std::string> tableNames;
    pugi::xml_document document;
    pugi::xml_parse_result const parsed = document.load_buffer(xml.data(), xml.size(), pugi::parse_default | pugi::parse_ws_pcdata_single, pugi::encoding_utf8);
    if (!parsed)
        throw std::invalid_argument(std::string("LatestFileList XML is not well-formed: ") + parsed.description());

    pugi::xml_node root = document.child("LatestFileList");
    if (!root)
        throw std::invalid_argument("LatestFileList XML is missing its root element");

    for (pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
    {
        if (std::string(node.name()) == "_TableList")
        {
            for (pugi::xml_node recordNode = node.first_child(); recordNode; recordNode = recordNode.next_sibling())
            {
                if (std::string(recordNode.name()) != "RECORD")
                    continue;
                for (pugi::xml_node field = recordNode.first_child(); field; field = field.next_sibling())
                {
                    if (std::string(field.name()) == "Name")
                        tableNames.push_back(Value(field));
                }
            }
        }
        else if (std::string(node.name()) == "About")
        {
            for (pugi::xml_node recordNode = node.first_child(); recordNode; recordNode = recordNode.next_sibling())
            {
                if (std::string(recordNode.name()) != "RECORD")
                    continue;
                for (pugi::xml_node field = recordNode.first_child(); field; field = field.next_sibling())
                {
                    if (std::string(field.name()) == "Version")
                        list.About.Version = ParseUint(field);
                }
            }
        }
        else if (std::string(node.name()) == "Package")
        {
            ReadPackage(node, list);
        }
    }

    if (!tableNames.empty() && tableNames != list.TableList())
        throw std::invalid_argument("LatestFileList XML table list does not match its tables");

    return list;
}
