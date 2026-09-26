/*
 * Project Ambrose by Imjustchico
 * One generation of the records made from a family of templates the client keys by the string hash of their names, as it keys spells and sigils: checked whole when built, since a record whose template id is not its name's hash is one the client cannot find and no id may appear twice, with each fault named up to MaxReportedErrors and the rest counted; found by id, by exact name through that hash as the client finds it, by name whatever its case, where two names differing only in case keep the lower id, by whichever of id or name a command was given, and searched by the text a name holds. A record gives its TemplateId, Name and File.
 */

#ifndef AMBROSE_NAMEKEYEDTEMPLATES_H
#define AMBROSE_NAMEKEYEDTEMPLATES_H

#include "StringHash.h"
#include "StringUtil.h"
#include "Types.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

template<class Record>
class NameKeyedTemplates
{
public:
    static constexpr std::size_t MaxReportedErrors = 100;

    NameKeyedTemplates() = default;
    NameKeyedTemplates(NameKeyedTemplates const&) = delete;
    NameKeyedTemplates& operator=(NameKeyedTemplates const&) = delete;

    static uint32 NameHash(std::string_view name) noexcept
    {
        return StringHash::KiStringHash(name);
    }

    static std::shared_ptr<NameKeyedTemplates const> Build(std::vector<Record> records, std::vector<std::string>& errors)
    {
        std::size_t faults = 0;
        auto const report = [&errors, &faults](std::string error)
        {
            if (++faults <= MaxReportedErrors)
                errors.push_back(std::move(error));
        };
        std::sort(records.begin(), records.end(), [](Record const& left, Record const& right) { return left.TemplateId < right.TemplateId; });
        auto built = std::make_shared<NameKeyedTemplates>();
        built->_byId.reserve(records.size());
        built->_byFoldedName.reserve(records.size());
        for (std::size_t index = 0; index < records.size(); ++index)
        {
            Record const& record = records[index];
            if (record.Name.empty())
                report(fmt::format("{} has no name", record.File));
            else if (NameHash(record.Name) != record.TemplateId)
                report(fmt::format("{} is listed as template {}, but its name {} hashes to {}, the id the client looks it up by", record.File, record.TemplateId, record.Name,
                    NameHash(record.Name)));
            auto const [existing, added] = built->_byId.emplace(record.TemplateId, index);
            if (!added)
                report(fmt::format("template {} is both {} and {}", record.TemplateId, records[existing->second].File, record.File));
            built->_byFoldedName.emplace(Ambrose::ToLower(record.Name), index);
        }
        if (faults > MaxReportedErrors)
            errors.push_back(fmt::format("and {} more problems", faults - MaxReportedErrors));
        if (faults != 0)
            return nullptr;
        built->_records = std::move(records);
        return built;
    }

    Record const* Find(uint32 templateId) const noexcept
    {
        auto const found = _byId.find(templateId);
        return found == _byId.end() ? nullptr : &_records[found->second];
    }

    Record const* FindByName(std::string_view name) const
    {
        if (Record const* const hashed = Find(NameHash(name)); hashed != nullptr && hashed->Name == name)
            return hashed;
        auto const folded = _byFoldedName.find(Ambrose::ToLower(name));
        return folded == _byFoldedName.end() ? nullptr : &_records[folded->second];
    }

    Record const* FindByIdOrName(std::string_view text) const
    {
        if (std::optional<uint32> const id = Ambrose::StringTo<uint32>(text))
            if (Record const* const found = Find(*id))
                return found;
        return FindByName(text);
    }

    std::vector<Record const*> Search(std::string_view text) const
    {
        std::string const wanted = Ambrose::ToLower(text);
        std::vector<Record const*> matches;
        for (Record const& record : _records)
            if (Ambrose::ToLower(record.Name).find(wanted) != std::string::npos)
                matches.push_back(&record);
        std::sort(matches.begin(), matches.end(), [](Record const* left, Record const* right) { return left->Name < right->Name; });
        return matches;
    }

    std::vector<Record> const& GetAll() const noexcept { return _records; }
    std::size_t Size() const noexcept { return _records.size(); }

private:
    std::vector<Record> _records;
    std::unordered_map<uint32, std::size_t> _byId;
    std::unordered_map<std::string, std::size_t> _byFoldedName;
};

#endif
