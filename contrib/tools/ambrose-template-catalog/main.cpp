/*
 * Project Ambrose by Imjustchico
 * Counts ObjectData templates by decoded class and reports the shared bases of the top one hundred classes.
 */

#include "ObjectTemplateMgr.h"
#include "PropertyObject.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using ClassCounts = std::map<std::string, std::size_t, std::less<>>;

    std::set<std::string> AncestorsOf(ClassInfo const& type)
    {
        std::set<std::string> ancestors;
        std::vector<ClassInfo const*> pending(type.Bases.begin(), type.Bases.end());
        while (!pending.empty())
        {
            ClassInfo const* const base = pending.back();
            pending.pop_back();
            if (base == nullptr || !ancestors.insert(base->Name).second)
                continue;
            pending.insert(pending.end(), base->Bases.begin(), base->Bases.end());
        }
        return ancestors;
    }

    std::set<std::string> DirectBasesOf(ClassInfo const& type)
    {
        std::set<std::string> bases;
        if (!type.Bases.empty() && type.Bases.front() != nullptr)
            bases.insert(type.Bases.front()->Name);
        return bases;
    }

    std::set<std::string> CommonSets(std::vector<ClassInfo const*> const& classes, bool ancestors)
    {
        std::set<std::string> common;
        if (classes.empty())
            return common;
        common = ancestors ? AncestorsOf(*classes.front()) : DirectBasesOf(*classes.front());
        for (std::size_t index = 1; index < classes.size(); ++index)
        {
            std::set<std::string> const current = ancestors ? AncestorsOf(*classes[index]) : DirectBasesOf(*classes[index]);
            std::erase_if(common, [&current](std::string const& name) { return !current.contains(name); });
        }
        return common;
    }

    int Fail(std::string_view message)
    {
        std::cerr << "error: " << message << '\n';
        return 1;
    }

    bool IsWithin(
        std::filesystem::path const& candidate,
        std::filesystem::path const& root,
        std::error_code& error)
    {
        std::filesystem::path const canonicalCandidate = std::filesystem::weakly_canonical(candidate, error);
        if (error)
            return false;
        std::filesystem::path const canonicalRoot = std::filesystem::weakly_canonical(root, error);
        if (error)
            return false;
        auto candidatePart = canonicalCandidate.begin();
        for (auto rootPart = canonicalRoot.begin(); rootPart != canonicalRoot.end(); ++rootPart, ++candidatePart)
        {
            if (candidatePart == canonicalCandidate.end() || _wcsicmp(candidatePart->c_str(), rootPart->c_str()) != 0)
                return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 5 || std::string_view(argv[3]) != "--output")
    {
        std::cerr << "usage: ambrose-template-catalog <client-install> <type-dump> --output <private-json-path>\n";
        return 2;
    }

    std::filesystem::path const install = argv[1];
    std::filesystem::path const typeDump = argv[2];
    std::filesystem::path const outputPath = argv[4];
    if (!std::filesystem::is_directory(install))
        return Fail("client install is not a directory");
    if (!std::filesystem::is_regular_file(typeDump))
        return Fail("type dump is not a file");
    std::error_code pathError;
    if (IsWithin(outputPath, install, pathError))
        return Fail("output must not be inside the client install");
    if (pathError)
        return Fail("output path could not be validated against the client install");
    if (IsWithin(outputPath, AMBROSE_SOURCE_DIR, pathError))
        return Fail("output must not be inside the Project Ambrose repository");
    if (pathError)
        return Fail("output path could not be validated against the Project Ambrose repository");
    if (!sTypeRegistry.LoadFromFile(typeDump))
        return Fail("type dump could not be loaded");

    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
        return Fail("type catalog was not created");

    ObjectTemplateMgr templates;
    templates.SetInstall(install);
    std::vector<std::string> errors;
    if (!templates.LoadManifest(errors))
    {
        for (std::string const& error : errors)
            std::cerr << "error: " << error << '\n';
        return 1;
    }

    ClassCounts counts;
    std::size_t objectDataTemplates = 0;
    for (auto const& [id, location] : templates.GetManifest()->GetLocations())
    {
        if (!std::string_view(location.Path).starts_with("ObjectData/"))
            continue;
        ++objectDataTemplates;
        TemplateLookup const found = templates.Lookup(id);
        if (!found.Template)
        {
            std::cerr << "error: template " << id << ": " << found.Error << '\n';
            return 1;
        }
        ++counts[found.Template->Object->GetClass().Name];
    }

    std::vector<std::pair<std::string, std::size_t>> ordered(counts.begin(), counts.end());
    std::sort(ordered.begin(), ordered.end(), [](auto const& left, auto const& right)
    {
        if (left.second != right.second)
            return left.second > right.second;
        return left.first < right.first;
    });
    std::vector<ClassInfo const*> topClasses;
    std::size_t const topCount = std::min<std::size_t>(100, ordered.size());
    nlohmann::json top = nlohmann::json::array();
    for (std::size_t index = 0; index < topCount; ++index)
    {
        ClassInfo const* const type = catalog->FindClass(ordered[index].first);
        if (type == nullptr)
            return Fail("a decoded template class is absent from the loaded type catalog");
        topClasses.push_back(type);
        top.push_back({
            { "class", ordered[index].first },
            { "count", ordered[index].second },
            { "direct_bases", DirectBasesOf(*type) },
            { "ancestors", AncestorsOf(*type) },
        });
    }

    nlohmann::json classCounts = nlohmann::json::array();
    for (auto const& [name, count] : ordered)
        classCounts.push_back({ { "class", name }, { "count", count } });

    nlohmann::json result = {
        { "object_data_templates", objectDataTemplates },
        { "template_classes", ordered.size() },
        { "top_class_count", topCount },
        { "class_counts", std::move(classCounts) },
        { "top_100", std::move(top) },
        { "common_direct_bases", CommonSets(topClasses, false) },
        { "common_ancestors", CommonSets(topClasses, true) },
    };
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output)
        return Fail("private output file could not be opened");
    output << result.dump(2) << '\n';
    if (!output)
        return Fail("private output file could not be written");
    std::cout << "catalog written to " << outputPath.string() << '\n';
    std::cerr << "cataloged " << objectDataTemplates << " ObjectData templates across " << counts.size() << " classes\n";
    return 0;
}
