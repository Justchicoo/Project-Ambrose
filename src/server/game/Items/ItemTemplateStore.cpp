/*
 * Project Ambrose by Imjustchico
 * Sorts the records by template id, indexes them by id and by folded object name, keeping the first of each, and counts the memory the records and both indexes take.
 */

#include "ItemTemplateStore.h"
#include "StringUtil.h"

#include <algorithm>
#include <optional>
#include <utility>

std::shared_ptr<ItemTemplateStore const> ItemTemplateStore::Build(std::vector<ItemTemplateRecord> records)
{
    std::sort(records.begin(), records.end(), [](ItemTemplateRecord const& left, ItemTemplateRecord const& right) { return left.TemplateId < right.TemplateId; });
    auto built = std::make_shared<ItemTemplateStore>();
    built->_byId.reserve(records.size());
    built->_byFoldedName.reserve(records.size());
    std::size_t memory = records.capacity() * sizeof(ItemTemplateRecord);
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        ItemTemplateRecord const& record = records[index];
        built->_byId.emplace(record.TemplateId, index);
        if (!record.ObjectName.empty())
            built->_byFoldedName.emplace(Ambrose::ToLower(record.ObjectName), index);
        memory += record.GetMemoryUsage() - sizeof(ItemTemplateRecord) + record.ObjectName.size() + 3 * sizeof(void*) + sizeof(uint32) + sizeof(std::size_t);
    }
    built->_memory = memory + sizeof(ItemTemplateStore);
    built->_records = std::move(records);
    return built;
}

ItemTemplateRecord const* ItemTemplateStore::Find(uint32 templateId) const noexcept
{
    auto const found = _byId.find(templateId);
    return found == _byId.end() ? nullptr : &_records[found->second];
}

ItemTemplateRecord const* ItemTemplateStore::FindByName(std::string_view name) const
{
    auto const found = _byFoldedName.find(Ambrose::ToLower(name));
    return found == _byFoldedName.end() ? nullptr : &_records[found->second];
}

ItemTemplateRecord const* ItemTemplateStore::FindByIdOrName(std::string_view text) const
{
    if (std::optional<uint32> const id = Ambrose::StringTo<uint32>(text))
        if (ItemTemplateRecord const* const found = Find(*id))
            return found;
    return FindByName(text);
}

std::vector<ItemTemplateRecord const*> ItemTemplateStore::Search(std::string_view text) const
{
    std::string const wanted = Ambrose::ToLower(text);
    std::vector<ItemTemplateRecord const*> matches;
    for (ItemTemplateRecord const& record : _records)
        if (Ambrose::ToLower(record.ObjectName).find(wanted) != std::string::npos)
            matches.push_back(&record);
    std::sort(matches.begin(), matches.end(), [](ItemTemplateRecord const* left, ItemTemplateRecord const* right) { return left->ObjectName < right->ObjectName; });
    return matches;
}

std::map<std::string, std::size_t> ItemTemplateStore::CountByClass() const
{
    std::map<std::string, std::size_t> counts;
    for (ItemTemplateRecord const& record : _records)
        ++counts[record.ClassName];
    return counts;
}

std::size_t ItemTemplateStore::CountUnknownBehaviors() const noexcept
{
    std::size_t count = 0;
    for (ItemTemplateRecord const& record : _records)
        count += record.UnknownBehaviors;
    return count;
}
