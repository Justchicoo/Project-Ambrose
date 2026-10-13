/*
 * Project Ambrose by Imjustchico
 * Gives a wizard's level, school and zone to the requirement engine, comparing names whatever their case; a wizard with no school known has no school of focus, and a class project is active when Events.ClassProjects names it.
 */

#include "WizardRequirementContext.h"
#include "Settings.h"
#include "StringUtil.h"

std::optional<double> WizardRequirementContext::GetMagicLevel(std::string_view) const
{
    return static_cast<double>(_level);
}

std::optional<bool> WizardRequirementContext::HasSchoolOfFocus(std::string_view school) const
{
    return !_school.empty() && Ambrose::EqualsIgnoreCase(school, _school);
}

std::optional<bool> WizardRequirementContext::IsInZone(std::string_view zoneName) const
{
    return Ambrose::EqualsIgnoreCase(zoneName, _zone);
}

std::optional<bool> WizardRequirementContext::IsClassProjectActive(std::string_view projectName) const
{
    if (projectName.empty())
        return false;
    std::string const running = sSettings.Get<std::string>("Events.ClassProjects");
    for (std::string_view const project : Ambrose::Tokenize(running, ',', false))
        if (Ambrose::EqualsIgnoreCase(Ambrose::Trim(project), projectName))
            return true;
    return false;
}
