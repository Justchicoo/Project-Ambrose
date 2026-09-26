/*
 * Project Ambrose by Imjustchico
 * Registers every built-in typed view, so each type dump the server loads must bind all of them.
 */

#include "ObjectViews.h"

void ObjectViews::RegisterAll(TypedViewRegistry& registry)
{
    registry.Add(WizardCharacterCreationInfoView::Definition);
    registry.Add(CoreObjectView::Definition);
    registry.Add(ClientObjectView::Definition);
    registry.Add(WizClientObjectView::Definition);
    registry.Add(CoreTemplateView::Definition);
    registry.Add(GameObjectTemplateView::Definition);
    registry.Add(WizItemTemplateView::Definition);
    registry.Add(TemplateManifestView::Definition);
    registry.Add(TemplateLocationView::Definition);
    registry.Add(RequirementListView::Definition);
    registry.Add(NamedEffectView::Definition);
}
