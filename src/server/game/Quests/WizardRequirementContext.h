/*
 * Project Ambrose by Imjustchico
 * Answers the requirement engine for a wizard in the world: its quest log's quests, goals and registry, its level as its magic level in every school, its school as its school of focus, the zone it stands in, and whether a class project is one the realm runs.
 */

#ifndef AMBROSE_WIZARDREQUIREMENTCONTEXT_H
#define AMBROSE_WIZARDREQUIREMENTCONTEXT_H

#include "QuestLogContext.h"
#include "Types.h"

#include <string>
#include <utility>

class WizardRequirementContext : public QuestLogContext
{
public:
    WizardRequirementContext(QuestLog const& log, std::string school, int32 level, std::string zone)
        : QuestLogContext(log), _school(std::move(school)), _level(level), _zone(std::move(zone)) { }

    std::optional<double> GetMagicLevel(std::string_view school) const override;
    std::optional<bool> HasSchoolOfFocus(std::string_view school) const override;
    std::optional<bool> IsInZone(std::string_view zoneName) const override;
    std::optional<bool> IsClassProjectActive(std::string_view projectName) const override;

private:
    std::string _school;
    int32 _level = 0;
    std::string _zone;
};

#endif
