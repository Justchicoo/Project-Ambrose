/*
 * Project Ambrose by Imjustchico
 * The client's data tables written in its DML XML, such as AnimationData/MasterAnimationList.xml: a root element whose _TableList names the tables, each table an element of that name holding RECORD elements, each field an element named by the field with its TYPE, whether it is the KEY and its value as text. Parse reads every listed table in document order and refuses a document that is not such XML or lists a table it does not hold.
 */

#ifndef AMBROSE_DMLTABLEFILE_H
#define AMBROSE_DMLTABLEFILE_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct DmlTableField
{
    std::string Name;
    std::string Type;
    bool Key = false;
    std::string Value;
};

struct DmlTableRecord
{
    std::vector<DmlTableField> Fields;

    DmlTableField const* Find(std::string_view name) const noexcept;
};

struct DmlTable
{
    std::string Name;
    std::vector<DmlTableRecord> Records;
};

class DmlTableFile
{
public:
    DmlTableFile() = delete;

    static std::optional<std::vector<DmlTable>> Parse(std::string_view text, std::string& error);
    static DmlTable const* Find(std::vector<DmlTable> const& tables, std::string_view name) noexcept;
};

#endif
