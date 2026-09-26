/*
 * Project Ambrose by Imjustchico
 * Reads a sigil through the sigil view, each circle through the circle view, where a null entry is refused since a duel cannot place anyone in it, and a combat sigil's engage radius, battlefield effects, shadow thresholds and PvP and PvE scalars and limits through the combat sigil view; and describes a sigil in lines: its name and where it comes from, its circles by kind, then for a combat sigil its engage radius and each game mode's scalars and limits.
 */

#include "SigilInfo.h"
#include "ObjectViews.h"
#include "PropertyObject.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace
{
    std::string LimitsText(char const* mode, SigilLimits const& limits)
    {
        return fmt::format("  {} damage, resist and pierce scalars {}, {}, {}; damage limit {} (k0 {}, n0 {}), resist limit {} (k0 {}, n0 {})", mode, limits.DamageScalar,
            limits.ResistScalar, limits.PierceScalar, limits.DamageLimit, limits.DamageK0, limits.DamageN0, limits.ResistLimit, limits.ResistK0, limits.ResistN0);
    }
}

std::optional<SigilInfo> SigilInfo::Read(PropertyObject const& object, uint32 templateId, std::string file, std::string& error)
{
    std::optional<SigilTemplateView> const view = SigilTemplateView::From(object);
    if (!view)
    {
        error = fmt::format("{} is a {}, which is not a SigilTemplate", file, object.GetClass().Name);
        return std::nullopt;
    }
    SigilInfo sigil;
    sigil.TemplateId = templateId;
    sigil.Class = object.GetClass().Name;
    sigil.File = std::move(file);
    sigil.Name = view->GetSigilName();
    sigil.Type = view->GetSigilType();
    sigil.UseState = view->IsUseState();
    std::size_t position = 0;
    for (PropertyValue const& entry : view->GetSubCircles())
    {
        std::optional<SigilSubCircleView> const circle = SigilSubCircleView::From(entry.AsObject());
        if (!circle)
        {
            error = fmt::format("circle {} of {} is not a SigilSubCircle", position, sigil.File);
            return std::nullopt;
        }
        sigil.Circles.push_back({ circle->GetLocationType(), circle->GetLocationPreference(), circle->GetRotation(), circle->GetRadius() });
        ++position;
    }
    if (std::optional<CombatSigilTemplateView> const combat = CombatSigilTemplateView::From(object))
    {
        sigil.Combat = true;
        sigil.EngageRadius = combat->GetEngageRadius();
        sigil.ShadowThresholdType = combat->GetShadowThresholdType();
        sigil.ShadowThresholdFactor = combat->GetShadowThresholdFactor();
        sigil.ShadowPipRatingFactor = combat->GetShadowPipRatingFactor();
        sigil.PvP = { combat->GetScalarDamagePvP(), combat->GetScalarResistPvP(), combat->GetScalarPiercePvP(), combat->GetDamageLimitPvP(), combat->GetDK0PvP(), combat->GetDN0PvP(),
            combat->GetResistLimitPvP(), combat->GetRK0PvP(), combat->GetRN0PvP() };
        sigil.PvE = { combat->GetScalarDamagePvE(), combat->GetScalarResistPvE(), combat->GetScalarPiercePvE(), combat->GetDamageLimitPvE(), combat->GetDK0PvE(), combat->GetDN0PvE(),
            combat->GetResistLimitPvE(), combat->GetRK0PvE(), combat->GetRN0PvE() };
        for (PropertyValue const& entry : combat->GetBattlefieldEffects())
        {
            PropertyObject const* const held = entry.AsObject();
            if (held == nullptr)
                continue;
            std::optional<SpellEffectInfo> effect = SpellEffectInfo::Read(*held, error);
            if (!effect)
            {
                error = fmt::format("{}: {}", sigil.File, error);
                return std::nullopt;
            }
            sigil.BattlefieldEffects.push_back(std::move(*effect));
        }
    }
    return sigil;
}

std::size_t SigilInfo::CountCircles(std::string_view locationType) const noexcept
{
    return static_cast<std::size_t>(std::count_if(Circles.begin(), Circles.end(), [locationType](SigilCircle const& circle) { return circle.LocationType == locationType; }));
}

std::vector<std::string> SigilInfo::Describe() const
{
    std::vector<std::string> lines;
    lines.push_back(fmt::format("{}, template {}, {}, a {}{}", Name, TemplateId, File, Class.starts_with("class ") ? Class.substr(6) : Class, Type.empty() ? "" : ", type " + Type));
    std::vector<std::string> kinds;
    for (SigilCircle const& circle : Circles)
        if (std::find(kinds.begin(), kinds.end(), circle.LocationType) == kinds.end())
            kinds.push_back(circle.LocationType);
    std::string circles = fmt::format("  {} circle(s)", Circles.size());
    for (std::string const& kind : kinds)
        circles += fmt::format(", {} {}", CountCircles(kind), kind.empty() ? std::string("unnamed") : kind);
    lines.push_back(std::move(circles));
    if (!Combat)
        return lines;
    lines.push_back(fmt::format("  engages at {}, {} battlefield effect(s), shadow threshold factor {}, shadow pip rating factor {}", EngageRadius, BattlefieldEffects.size(),
        ShadowThresholdFactor, ShadowPipRatingFactor));
    lines.push_back(LimitsText("PvP", PvP));
    lines.push_back(LimitsText("PvE", PvE));
    return lines;
}
