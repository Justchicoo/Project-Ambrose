/*
 * Project Ambrose by Imjustchico
 * Fills WizardCharacterCreationInfo from a stored wizard: template 1, the custom name as UTF-16, guid and account, rename flag, last logout clamped to 32 bits, zone display as the location, level, world, school and name parts, a WizardCharacterBehavior with every appearance field and an empty EquippedItemInfoList; names the first property that refuses its value, and encodes the result as MSG_CHARACTERINFO.CharacterInfo.
 */

#include "LoginScreenInfoBuilder.h"
#include "AvatarAppearance.h"
#include "ObjectFields.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <string_view>

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

    PropertyObjectPtr Create(TypeCatalogPtr const& catalog, std::string_view className, std::string& problem)
    {
        PropertyObjectPtr object = PropertyObject::Create(catalog, className);
        if (!object && problem.empty())
            problem = fmt::format("the type dump has no property class {}", className);
        return object;
    }
}

PropertyObjectPtr LoginScreenInfoBuilder::Build(TypeCatalogPtr const& catalog, CharacterSummary const& character, std::string& problem)
{
    problem.clear();
    if (!catalog)
    {
        problem = "no type dump is loaded";
        return nullptr;
    }
    PropertyObjectPtr info = Create(catalog, "class WizardCharacterCreationInfo", problem);
    PropertyObjectPtr behavior = Create(catalog, "class WizardCharacterBehavior", problem);
    PropertyObjectPtr equipment = Create(catalog, "class EquippedItemInfoList", problem);
    if (!info || !behavior || !equipment)
        return nullptr;

    std::u16string name;
    if (character.CustomName)
    {
        std::optional<std::u16string> converted = Utf::Utf8ToUtf16(*character.CustomName, Utf::InvalidPolicy::Reject);
        if (!converted)
        {
            problem = fmt::format("character {} has a custom name that is not UTF-8", character.Guid);
            return nullptr;
        }
        name = std::move(*converted);
    }

    AvatarAppearance::Write(*behavior, character.Appearance, problem);
    Filler(*equipment, problem).Set("m_infoList", PropertyValue::List());

    uint32 const lastLogin = static_cast<uint32>(std::min<uint64>(character.LastLogout, std::numeric_limits<uint32>::max()));
    Filler(*info, problem)
        .Set("m_templateID", TemplateId)
        .Set("m_name", std::move(name))
        .Set("m_shouldRename", character.ShouldRename)
        .Set("m_globalID", character.Guid)
        .Set("m_userID", character.Account)
        .Set("m_quarantined", false)
        .Set("m_lastLoginTime", lastLogin)
        .Set("m_avatarBehavior", std::move(behavior))
        .Set("m_equipmentInfoList", std::move(equipment))
        .Set("m_location", character.ZoneDisplay)
        .Set("m_level", character.Level)
        .Set("m_world", character.World)
        .Set("m_schoolOfFocus", character.SchoolId)
        .Set("m_nameIndices", character.NameIndices);
    if (!problem.empty())
        return nullptr;
    return info;
}

EncodeResult LoginScreenInfoBuilder::Encode(TypeCatalogPtr const& catalog, CharacterSummary const& character)
{
    std::string problem;
    PropertyObjectPtr const info = Build(catalog, character, problem);
    if (!info)
    {
        EncodeResult failed;
        failed.Status = SerializerStatus::UnknownClass;
        failed.Detail = std::move(problem);
        return failed;
    }
    ObjectField const* const field = ObjectFields::Find("MSG_CHARACTERINFO", "CharacterInfo");
    return ObjectSerializer::EncodeField(*field, info.get());
}
