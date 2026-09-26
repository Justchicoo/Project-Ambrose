/*
 * Project Ambrose by Imjustchico
 * Reads every template TemplateManifest.xml lists under one folder of the install, such as Spells/ or Sigils/, for a manager that keeps its own typed records of them: the entries in template id order, each archive they name opened once and afresh so a reload reads the files as they are now, and the templates decoded on as many threads as the machine has, no more than one for every EntriesPerThread, each handed to the manager's reader by its position. The templates that fail are grouped by what went wrong once each file's own path is taken out, and each group is named by its first template with a count of the rest.
 */

#ifndef AMBROSE_TEMPLATEFOLDER_H
#define AMBROSE_TEMPLATEFOLDER_H

#include "TemplateManifest.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class PropertyObject;

struct TemplateFolderEntry
{
    uint32 Id = 0;
    TemplateLocation Location;
};

class TemplateFolder
{
public:
    static constexpr std::size_t EntriesPerThread = 256;
    static constexpr std::size_t MaxReportedFailures = 100;

    using Reader = std::function<bool(std::size_t index, TemplateFolderEntry const& entry, PropertyObject const& object, std::string& error)>;

    static std::vector<TemplateFolderEntry> List(TemplateManifest const& manifest, std::string_view folder);
    static bool ReadAll(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, std::vector<TemplateFolderEntry> const& entries, std::string_view what,
        std::string_view folder, Reader const& reader, std::vector<std::string>& errors, std::size_t& threads);
};

#endif
