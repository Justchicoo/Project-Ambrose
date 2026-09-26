/*
 * Project Ambrose by Imjustchico
 * A sigil as the server uses it, read once from the SigilTemplate the user's install holds through its typed views: its name, which the client hashes into its template id, its type, the circles a duel places its participants in, each a monster or player circle with its slot, angle and radius, and, for a combat sigil, the radius it engages at, the effects it lays on the battlefield, its shadow thresholds and the damage, resist and pierce scalars and limits it applies in PvP and in PvE.
 */

#ifndef AMBROSE_SIGILINFO_H
#define AMBROSE_SIGILINFO_H

#include "SpellInfo.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PropertyObject;

struct SigilCircle
{
    std::string LocationType;
    std::string LocationPreference;
    float Rotation = 0.0f;
    float Radius = 0.0f;
};

struct SigilLimits
{
    float DamageScalar = 0.0f;
    float ResistScalar = 0.0f;
    float PierceScalar = 0.0f;
    float DamageLimit = 0.0f;
    float DamageK0 = 0.0f;
    float DamageN0 = 0.0f;
    float ResistLimit = 0.0f;
    float ResistK0 = 0.0f;
    float ResistN0 = 0.0f;
};

struct SigilInfo
{
    uint32 TemplateId = 0;
    std::string Class;
    std::string File;
    std::string Name;
    std::string Type;
    bool UseState = false;
    std::vector<SigilCircle> Circles;
    bool Combat = false;
    float EngageRadius = 0.0f;
    std::vector<SpellEffectInfo> BattlefieldEffects;
    int64 ShadowThresholdType = 0;
    float ShadowThresholdFactor = 0.0f;
    float ShadowPipRatingFactor = 0.0f;
    SigilLimits PvP;
    SigilLimits PvE;

    static std::optional<SigilInfo> Read(PropertyObject const& object, uint32 templateId, std::string file, std::string& error);
    std::size_t CountCircles(std::string_view locationType) const noexcept;
    std::vector<std::string> Describe() const;
};

#endif
