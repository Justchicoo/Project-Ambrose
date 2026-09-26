/*
 * Project Ambrose by Imjustchico
 * Reads a spell and each of its effects through the spell views, finding an effect's own effects in any object or list it holds, and one level inside an object it holds that is not an effect, the way a conditional element holds its effect behind its requirements, down to MaxEffectDepth; and describes a spell in lines: its name and where it comes from, then its school, rank, pips, accuracy and type, then each effect indented under the one that holds it, its type, target and disposition named through the enum options the catalog gives SpellEffect.
 */

#include "SpellInfo.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <string_view>
#include <utility>

namespace
{
    template<typename Visit>
    void ForEachHeldObject(PropertyObject const& object, Visit&& visit)
    {
        std::size_t const count = object.GetClass().Properties.size();
        for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
        {
            PropertyValue const* const value = object.GetAt(ordinal);
            if (value == nullptr)
                continue;
            if (PropertyObject const* const held = value->AsObject())
            {
                visit(*held);
                continue;
            }
            if (PropertyValue::List const* const list = value->GetList())
                for (PropertyValue const& item : *list)
                    if (PropertyObject const* const held = item.AsObject())
                        visit(*held);
        }
    }

    bool ReadEffect(PropertyObject const& object, bool conditional, std::size_t depth, SpellEffectInfo& effect, std::string& error)
    {
        std::optional<SpellEffectView> const view = SpellEffectView::From(object);
        if (!view)
        {
            error = fmt::format("an effect is a {}, which is not a SpellEffect", object.GetClass().Name);
            return false;
        }
        if (depth > SpellInfo::MaxEffectDepth)
        {
            error = fmt::format("its effects hold one another deeper than {}", SpellInfo::MaxEffectDepth);
            return false;
        }
        effect.Class = object.GetClass().Name;
        effect.Type = view->GetEffectType();
        effect.Param = view->GetEffectParam();
        effect.Disposition = view->GetDisposition();
        effect.DamageType = view->GetDamageTypeName();
        effect.DamageTypeId = view->GetDamageType();
        effect.PipNum = view->GetPipNum();
        effect.ActNum = view->GetActNum();
        effect.Target = view->GetEffectTarget();
        effect.NumRounds = view->GetNumRounds();
        effect.ParamPerRound = view->GetParamPerRound();
        effect.HealModifier = view->GetHealModifier();
        effect.SpellTemplateId = view->GetSpellTemplateId();
        effect.EnchantmentSpellTemplateId = view->GetEnchantmentSpellTemplateId();
        effect.ArmorPiercing = view->GetArmorPiercingParam();
        effect.ChancePerTarget = view->GetChancePerTarget();
        effect.Rank = view->GetRank();
        effect.Cloaked = view->IsCloaked();
        effect.Conditional = conditional;

        bool ok = true;
        auto const adopt = [&](PropertyObject const& held, bool behindSomething)
        {
            SpellEffectInfo child;
            ok = ReadEffect(held, behindSomething, depth + 1, child, error);
            if (ok)
                effect.Effects.push_back(std::move(child));
        };
        ForEachHeldObject(object, [&](PropertyObject const& held)
        {
            if (!ok)
                return;
            if (SpellEffectView::From(held))
            {
                adopt(held, false);
                return;
            }
            ForEachHeldObject(held, [&](PropertyObject const& inner)
            {
                if (ok && SpellEffectView::From(inner))
                    adopt(inner, true);
            });
        });
        return ok;
    }

    std::size_t Count(std::vector<SpellEffectInfo> const& effects) noexcept
    {
        std::size_t total = effects.size();
        for (SpellEffectInfo const& effect : effects)
            total += Count(effect.Effects);
        return total;
    }

    struct EffectNames
    {
        PropertyInfo const* Type = nullptr;
        PropertyInfo const* Target = nullptr;
        PropertyInfo const* Disposition = nullptr;

        static std::string Name(PropertyInfo const* property, int64 value)
        {
            if (property != nullptr)
                if (std::optional<std::string_view> const name = property->FindOptionName(value))
                    return std::string(*name);
            return std::to_string(value);
        }
    };

    std::string ShortClass(std::string_view name)
    {
        return std::string(name.starts_with("class ") ? name.substr(6) : name);
    }

    void DescribeEffects(std::vector<SpellEffectInfo> const& effects, EffectNames const& names, std::size_t depth, std::vector<std::string>& lines)
    {
        for (SpellEffectInfo const& effect : effects)
        {
            std::string line = fmt::format("{:{}}{} {}", "", 2 * depth, EffectNames::Name(names.Type, effect.Type), effect.Param);
            if (!effect.DamageType.empty())
                line += " " + effect.DamageType;
            if (effect.NumRounds != 0)
                line += fmt::format(" over {} rounds, {} a round", effect.NumRounds, effect.ParamPerRound);
            line += " to " + EffectNames::Name(names.Target, effect.Target);
            if (effect.Disposition != 0)
                line += ", " + EffectNames::Name(names.Disposition, effect.Disposition);
            if (effect.SpellTemplateId != 0)
                line += fmt::format(", spell {}", effect.SpellTemplateId);
            if (effect.Conditional)
                line += ", when its requirements are met";
            if (effect.Class != "class SpellEffect")
                line += fmt::format(", a {}{}", ShortClass(effect.Class), effect.Effects.empty() ? "" : " of:");
            lines.push_back(std::move(line));
            DescribeEffects(effect.Effects, names, depth + 1, lines);
        }
    }
}

std::optional<SpellEffectInfo> SpellEffectInfo::Read(PropertyObject const& object, std::string& error)
{
    SpellEffectInfo effect;
    if (!ReadEffect(object, false, 1, effect, error))
        return std::nullopt;
    return effect;
}

std::optional<SpellInfo> SpellInfo::Read(PropertyObject const& object, uint32 templateId, std::string file, std::string& error)
{
    std::optional<SpellTemplateView> const view = SpellTemplateView::From(object);
    if (!view)
    {
        error = fmt::format("{} is a {}, which is not a SpellTemplate", file, object.GetClass().Name);
        return std::nullopt;
    }
    SpellInfo spell;
    spell.TemplateId = templateId;
    spell.Class = object.GetClass().Name;
    spell.File = std::move(file);
    spell.Name = view->GetName();
    spell.DisplayKey = view->GetDisplayName();
    spell.DescriptionKey = view->GetDescription();
    spell.Base = view->GetSpellBase();
    spell.School = view->GetMagicSchoolName();
    spell.SecondarySchool = view->GetSecondarySchoolName();
    spell.RequiredSchool = view->GetRequiredSchoolName();
    spell.TypeName = view->GetTypeName();
    spell.Accuracy = view->GetAccuracy();
    spell.TrainingCost = view->GetTrainingCost();
    spell.LevelRestriction = view->GetLevelRestriction();
    spell.SourceType = view->GetSpellSourceType();
    spell.Treasure = view->IsTreasure();
    spell.PvP = view->IsPvP();
    spell.PvE = view->IsPvE();
    if (std::optional<SpellRankView> const rank = SpellRankView::From(view->GetSpellRank()))
        spell.Pips = { rank->GetRank(), rank->GetBalancePips(), rank->GetDeathPips(), rank->GetFirePips(), rank->GetIcePips(), rank->GetLifePips(), rank->GetMythPips(),
            rank->GetStormPips(), rank->GetShadowPips(), rank->IsXPipSpell() };
    for (PropertyValue const& entry : view->GetEffects())
    {
        PropertyObject const* const held = entry.AsObject();
        if (held == nullptr)
            continue;
        SpellEffectInfo effect;
        if (!ReadEffect(*held, false, 1, effect, error))
        {
            error = fmt::format("{}: {}", spell.File, error);
            return std::nullopt;
        }
        spell.Effects.push_back(std::move(effect));
    }
    return spell;
}

std::vector<std::string> SpellInfo::Describe(TypeCatalog const* catalog) const
{
    EffectNames names;
    if (catalog != nullptr)
        if (ClassInfo const* const effect = catalog->FindClass("class SpellEffect"))
            names = { effect->FindProperty("m_effectType"), effect->FindProperty("m_effectTarget"), effect->FindProperty("m_disposition") };

    std::vector<std::string> lines;
    lines.push_back(fmt::format("{}, template {}, {}, a {}", Name, TemplateId, File, ShortClass(Class)));
    std::string pips = fmt::format("rank {}{}", Pips.Rank, Pips.X ? " X" : "");
    std::pair<char const*, uint8> const schoolPips[] = { { "balance", Pips.Balance }, { "death", Pips.Death }, { "fire", Pips.Fire }, { "ice", Pips.Ice }, { "life", Pips.Life },
        { "myth", Pips.Myth }, { "storm", Pips.Storm }, { "shadow", Pips.Shadow } };
    for (auto const& [school, count] : schoolPips)
        if (count != 0)
            pips += fmt::format(", {} {}", count, school);
    lines.push_back(fmt::format("  school {}{}, {}, accuracy {}%, {}{}", School.empty() ? std::string("none") : School,
        SecondarySchool.empty() ? std::string() : " and " + SecondarySchool, pips, Accuracy, TypeName.empty() ? std::string("no type") : "type " + TypeName,
        Treasure ? ", a treasure card" : ""));
    if (Effects.empty())
        lines.push_back("  no effects");
    DescribeEffects(Effects, names, 1, lines);
    return lines;
}

std::size_t SpellInfo::CountEffects() const noexcept
{
    return Count(Effects);
}
