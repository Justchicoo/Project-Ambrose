/*
 * Project Ambrose by Imjustchico
 * What the Studio shows, read once when it starts: the world database's quest rows, the user's own Root.wad and the type dump that decodes both, answered as JSON pages of quests and cinematics, one quest with its goals, dialogs and decoded requirements and results, and one cinematic decoded from the install with every quest result that names it, so a quest step and the cutscene it plays are linked in both directions.
 */

#ifndef AMBROSE_STUDIOCONTENT_H
#define AMBROSE_STUDIOCONTENT_H

#include "KiwadArchive.h"
#include "QuestRows.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct StudioCinematicUse
{
    std::string Quest;
    std::string Goal;
    std::string Class;
    bool Start = false;
};

class StudioContent
{
public:
    static constexpr std::string_view CinematicFolder = "Cinematics/";
    static constexpr std::size_t MaxPage = 200;

    using TextLookup = std::function<std::optional<std::string>(std::string_view key)>;

    StudioContent(QuestRows rows, TypeCatalogPtr catalog, std::shared_ptr<KiwadArchive const> root, TextLookup text);

    nlohmann::ordered_json Summary() const;
    nlohmann::ordered_json Quests(std::string_view search, std::size_t offset, std::size_t limit) const;
    std::optional<nlohmann::ordered_json> Quest(std::string_view name) const;
    nlohmann::ordered_json Cinematics(std::string_view search, std::size_t offset, std::size_t limit) const;
    std::optional<nlohmann::ordered_json> Cinematic(std::string_view name, std::string& error) const;

    static std::string CinematicKey(std::string_view text);

private:
    nlohmann::ordered_json Decode(std::vector<uint8> const& bytes) const;
    nlohmann::ordered_json DecodeEntry(std::vector<uint8> const& bytes, std::string& error) const;
    std::string Text(std::string const& key) const;
    std::vector<std::string> NamedCinematics(nlohmann::ordered_json const& value) const;
    void IndexCinematics();

    QuestRows _rows;
    TypeCatalogPtr _catalog;
    std::shared_ptr<KiwadArchive const> _root;
    TextLookup _text;
    std::vector<std::string> _cinematics;
    std::map<std::string, std::string, std::less<>> _cinematicByKey;
    std::map<std::string, std::vector<StudioCinematicUse>, std::less<>> _uses;
};

#endif
