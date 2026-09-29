/*
 * Project Ambrose by Imjustchico
 * Reading and replacing a root's protected patterns. Every line is compiled with the same rules the jail matches with before anything is written, so a pattern that could never match is refused with its line rather than kept to confuse somebody later, and a replace deletes the root's rows and writes the new ones in order within the caller's transaction.
 */

#include "PanelFileRules.h"

#include "PanelStore.h"
#include "PathRules.h"

#include <fmt/format.h>

namespace
{
    std::string Cleaned(std::string const& line)
    {
        std::string text = line;
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n'))
            text.pop_back();
        return text;
    }
}

PanelFileRules::PanelFileRules(PanelStore& store) : _store(store)
{
}

std::vector<std::string> PanelFileRules::Normalise(std::vector<std::string> const& lines)
{
    std::vector<std::string> patterns;
    for (std::string const& line : lines)
    {
        std::string const text = Cleaned(line);
        if (!Ambrose::PathRules::IsComment(text))
            patterns.push_back(text);
    }
    return patterns;
}

std::vector<PanelFileRuleProblem> PanelFileRules::Validate(std::vector<std::string> const& lines)
{
    std::vector<PanelFileRuleProblem> problems;
    std::size_t kept = 0;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        std::string const text = Cleaned(lines[index]);
        if (Ambrose::PathRules::IsComment(text))
            continue;
        ++kept;
        std::string error;
        if (!Ambrose::PathRules::Compile(text, Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, {}, error))
            problems.push_back({ index + 1, error });
    }
    if (kept > MaxPatterns)
        problems.push_back({ 0, fmt::format("A root holds at most {} protected patterns; this list has {}", MaxPatterns, kept) });
    return problems;
}

bool PanelFileRules::Read(std::map<std::string, std::vector<std::string>, std::less<>>& rules, std::string& error) const
{
    rules.clear();
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT root, pattern FROM panel_file_rule ORDER BY root, position", error);
    if (!rows)
        return false;
    while (rows->Step(error))
        rules[rows->Text(0)].push_back(rows->Text(1));
    return error.empty();
}

bool PanelFileRules::ReadRoot(std::string_view root, std::vector<std::string>& patterns, std::string& error) const
{
    patterns.clear();
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT pattern FROM panel_file_rule WHERE root = ?1 ORDER BY position", error);
    if (!rows)
        return false;
    rows->Bind(1, root);
    while (rows->Step(error))
        patterns.push_back(rows->Text(0));
    return error.empty();
}

bool PanelFileRules::Replace(std::string_view root, std::vector<std::string> const& patterns, std::optional<int64> by, std::string& error)
{
    std::optional<PanelStore::Statement> remove = _store.Prepare("DELETE FROM panel_file_rule WHERE root = ?1", error);
    if (!remove)
        return false;
    remove->Bind(1, root);
    if (!remove->Run(error))
        return false;
    remove.reset();
    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT INTO panel_file_rule (root, position, pattern, created_epoch_ms, created_by) VALUES (?1, ?2, ?3, ?4, ?5)", error);
    if (!insert)
        return false;
    int64 const now = PanelStore::NowEpochMs();
    for (std::size_t index = 0; index < patterns.size(); ++index)
    {
        insert->Reset();
        insert->Bind(1, root);
        insert->Bind(2, static_cast<int64>(index));
        insert->Bind(3, patterns[index]);
        insert->Bind(4, now);
        if (by)
            insert->Bind(5, *by);
        else
            insert->BindNull(5);
        if (!insert->Run(error))
            return false;
    }
    return true;
}
