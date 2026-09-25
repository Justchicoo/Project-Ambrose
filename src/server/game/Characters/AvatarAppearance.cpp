/*
 * Project Ambrose by Imjustchico
 * Sets each WizardCharacterBehavior appearance property from the stored look, stopping at the first one the behavior refuses and saying which it was and why.
 */

#include "AvatarAppearance.h"

#include <fmt/format.h>

#include <string_view>
#include <utility>

namespace
{
    class Filler
    {
    public:
        Filler(PropertyObject& object, std::string& problem) : _object(object), _problem(problem)
        {
        }

        Filler& Set(std::string_view name, PropertyValue&& value)
        {
            if (!_problem.empty())
                return *this;
            PropertySetResult const result = _object.Set(name, std::move(value));
            if (result != PropertySetResult::Ok)
                _problem = fmt::format("{} property {} refused its value: {}", _object.GetClass().Name, name, PropertyObject::GetResultName(result));
            return *this;
        }

    private:
        PropertyObject& _object;
        std::string& _problem;
    };
}

bool AvatarAppearance::Write(PropertyObject& behavior, CharacterAppearance const& look, std::string& problem)
{
    Filler(behavior, problem)
        .Set("m_behaviorTemplateNameID", look.BehaviorTemplateNameId)
        .Set("m_nHeadHandsModel", uint32{ look.HeadHandsModel })
        .Set("m_nHairModel", uint32{ look.HairModel })
        .Set("m_nHatModel", uint32{ look.HatModel })
        .Set("m_nTorsoModel", uint32{ look.TorsoModel })
        .Set("m_nFeetModel", uint32{ look.FeetModel })
        .Set("m_nWandModel", uint32{ look.WandModel })
        .Set("m_nSkinColor", uint32{ look.SkinColor })
        .Set("m_nSkinDecal", uint32{ look.SkinDecal })
        .Set("m_nHairColor", uint32{ look.HairColor })
        .Set("m_nHatColor", uint32{ look.HatColor })
        .Set("m_nHatDecal", uint32{ look.HatDecal })
        .Set("m_nTorsoColor", uint32{ look.TorsoColor })
        .Set("m_nTorsoDecal", uint32{ look.TorsoDecal })
        .Set("m_nTorsoDecal2", uint32{ look.TorsoDecal2 })
        .Set("m_nFeetColor", uint32{ look.FeetColor })
        .Set("m_nFeetDecal", uint32{ look.FeetDecal })
        .Set("m_eGender", int64{ look.Gender })
        .Set("m_eRace", int64{ look.Race })
        .Set("m_afterCombatDance", look.AfterCombatDance)
        .Set("m_nSkinDecal2", look.SkinDecal2)
        .Set("m_extendedHairColor", look.ExtendedHairColor)
        .Set("m_extendedSkinDecal", look.ExtendedSkinDecal)
        .Set("m_newPlayerOptions", look.NewPlayerOptions)
        .Set("m_newPlayerOptions2", look.NewPlayerOptions2)
        .Set("m_afterCombatVictoryDance", look.AfterCombatVictoryDance);
    return problem.empty();
}
