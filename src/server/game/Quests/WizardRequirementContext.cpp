/*
 * Project Ambrose by Imjustchico
 * Gives a wizard's level, school and zone to the requirement engine, comparing names whatever their case; a wizard with no school known has no school of focus.
 */

#include "WizardRequirementContext.h"
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
