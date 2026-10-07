/*
 * Project Ambrose by Imjustchico
 * The typed record of one item template the user's install holds, any template whose class is or derives from WizItemTemplate: its id, class and file, its object name, display key, object type and adjectives as a game object template carries them, and its school, base cost, rank, item limit, set bonus and color counts. A template whose fields hold an object of a class the type dump does not list is refused, naming the class hash and where it sits, while one in its behaviors keeps its place and is counted, as every template's behaviors are.
 */

#ifndef AMBROSE_ITEMTEMPLATERECORD_H
#define AMBROSE_ITEMTEMPLATERECORD_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PropertyObject;
struct DecodeIssue;

struct ItemTemplateRecord
{
    static constexpr std::string_view ItemClass = "class WizItemTemplate";

    uint32 TemplateId = 0;
    std::string ClassName;
    std::string File;
    std::string ObjectName;
    std::string DisplayKey;
    std::optional<int64> ObjectType;
    std::vector<std::string> Adjectives;
    std::string School;
    float BaseCost = 0.0f;
    int32 Rank = 0;
    int32 ItemLimit = 0;
    uint32 ItemSetBonusTemplateId = 0;
    std::optional<int64> NumPrimaryColors;
    std::optional<int64> NumSecondaryColors;
    std::size_t UnknownBehaviors = 0;

    static bool IsItem(PropertyObject const& object) noexcept;
    static std::optional<ItemTemplateRecord> Read(PropertyObject const& object, uint32 templateId, std::string file, std::vector<DecodeIssue> const& issues, std::string& error);

    std::size_t GetMemoryUsage() const noexcept;
};

#endif
