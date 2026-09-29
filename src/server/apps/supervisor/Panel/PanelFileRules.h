/*
 * Project Ambrose by Imjustchico
 * The protected path patterns an owner adds to a file root beside its built-in ones, kept in the panel store's panel_file_rule table in the order they were written: blank lines and comments are dropped before anything is kept, every other line must compile as a gitignore pattern and a root holds at most 256, a refusal naming each line that failed and why, and a root's patterns are replaced whole inside the transaction that records the change, so the rules and their audit row land together or not at all.
 */

#ifndef AMBROSE_PANELFILERULES_H
#define AMBROSE_PANELFILERULES_H

#include "Types.h"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;

struct PanelFileRuleProblem
{
    std::size_t Line = 0;
    std::string Message = {};
};

class PanelFileRules
{
public:
    static constexpr std::size_t MaxPatterns = 256;

    explicit PanelFileRules(PanelStore& store);

    PanelFileRules(PanelFileRules const&) = delete;
    PanelFileRules& operator=(PanelFileRules const&) = delete;

    static std::vector<std::string> Normalise(std::vector<std::string> const& lines);
    static std::vector<PanelFileRuleProblem> Validate(std::vector<std::string> const& lines);

    bool Read(std::map<std::string, std::vector<std::string>, std::less<>>& rules, std::string& error) const;
    bool ReadRoot(std::string_view root, std::vector<std::string>& patterns, std::string& error) const;
    bool Replace(std::string_view root, std::vector<std::string> const& patterns, std::optional<int64> by, std::string& error);

private:
    PanelStore& _store;
};

#endif
