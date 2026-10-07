/*
 * Project Ambrose by Imjustchico
 * Checks the quest table rows before they are served: every name a start goal, logic entry or dialog uses is a goal of its quest, no logic entry both adds goals and completes the quest, a quest that has a starter has a Prep dialog, every actor template is in object_template and every persona names an object there, a bounty goal has an adjective and a tally count, and every key resolves when the locale text is loaded; each problem names the quest it belongs to.
 */

#ifndef AMBROSE_QUESTVALIDATOR_H
#define AMBROSE_QUESTVALIDATOR_H

#include "QuestRows.h"
#include "Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct QuestValidationError
{
    std::string Quest;
    std::string Message;
};

class QuestValidator
{
public:
    using KeyLookup = std::function<bool(std::string_view)>;

    struct Context
    {
        std::unordered_set<uint32> TemplateIds;
        std::unordered_set<std::string> ObjectNames;
        KeyLookup HasKey;

        static Context From(std::vector<QuestObjectRow> const& objects, KeyLookup hasKey);
        bool HasObjects() const noexcept { return !TemplateIds.empty(); }
    };

    static std::vector<QuestValidationError> Validate(QuestRows const& rows, Context const& context);
};

#endif
