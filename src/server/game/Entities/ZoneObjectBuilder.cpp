/*
 * Project Ambrose by Imjustchico
 * Makes a zone object from the catalog's own classes and defaults and sets only what the zone and the instance decide. The class and header come from the template's class through the core object table, so a template class no row gives a core type is refused rather than sent as a class the client would not build from it; a behavior the template names that behavior_client_class does not know refuses the build rather than being guessed or dropped, because the client pairs behaviors with the template's by position and one missing slot shifts every later one, and a slot the template itself leaves empty stays empty.
 */

#include "ZoneObjectBuilder.h"
#include "PropertyFiller.h"

#include <fmt/format.h>

#include <optional>
#include <utility>

PropertyObjectPtr ZoneObjectBuilder::Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, BehaviorClientClasses const& behaviors, ObjectTemplate const& objectTemplate,
    ZoneObjectPlacement const& placement, std::string& problem)
{
    problem.clear();
    if (!catalog)
    {
        problem = "no type dump is loaded";
        return nullptr;
    }
    if (!objectTemplate.IsLoaded() || !objectTemplate.Object)
    {
        problem = fmt::format("template {} has not been read from the install", objectTemplate.TemplateId);
        return nullptr;
    }
    std::optional<CoreObjectHeader> const header = types.HeaderFor(objectTemplate.Object->GetClass(), objectTemplate.TemplateId);
    CoreObjectType const* const built = header ? types.Find(header->Block) : nullptr;
    if (!header || !built)
    {
        problem = fmt::format("template {} is a {}, which core_template_type gives no core type, so the client would build nothing from it", objectTemplate.TemplateId,
            objectTemplate.Object->GetClass().Name);
        return nullptr;
    }
    PropertyObjectPtr object = PropertyObject::Create(catalog, built->ClassName);
    if (!object)
    {
        problem = fmt::format("the type dump has no property class {}", built->ClassName);
        return nullptr;
    }
    object->SetCoreHeader(*header);

    PropertyValue::List inactive;
    inactive.reserve(objectTemplate.Behaviors.size());
    for (std::string const& behaviorName : objectTemplate.Behaviors)
    {
        if (behaviorName.empty())
        {
            inactive.emplace_back(PropertyObjectPtr());
            continue;
        }
        BehaviorClientClass const* const row = behaviors.Find(behaviorName);
        if (!row)
        {
            problem = fmt::format("template {} names behavior {}, which behavior_client_class does not list, so the class the client builds for it is not known",
                objectTemplate.TemplateId, behaviorName);
            return nullptr;
        }
        if (!row->ClassName)
        {
            inactive.emplace_back(PropertyObjectPtr());
            continue;
        }
        PropertyObjectPtr behavior = PropertyObject::Create(catalog, *row->ClassName);
        if (!behavior)
        {
            problem = fmt::format("the type dump has no property class {}", *row->ClassName);
            return nullptr;
        }
        inactive.emplace_back(std::move(behavior));
    }

    PropertyFiller(*object, problem)
        .Set("m_inactiveBehaviors", std::move(inactive))
        .Set("m_globalID.m_full", placement.GlobalId)
        .Set("m_permID", placement.PermId)
        .Set("m_location", placement.Location)
        .Set("m_orientation", placement.Orientation)
        .Set("m_fScale", placement.Scale)
        .Set("m_templateID.m_full", uint64{ objectTemplate.TemplateId })
        .Set("m_nMobileID", placement.MobileId);
    if (!problem.empty())
        return nullptr;
    return object;
}
