/*
 * Project Ambrose by Imjustchico
 * Adds the zone data, location, object info and spawn data views to a view registry before its type dump loads.
 */

#include "ZoneViews.h"

void ZoneViews::RegisterAll(TypedViewRegistry& registry)
{
    registry.Add(WizZoneDataView::Definition);
    registry.Add(LocationTemplateView::Definition);
    registry.Add(CoreObjectInfoView::Definition);
    registry.Add(SpawnManagerView::Definition);
    registry.Add(SpawnObjectView::Definition);
    registry.Add(SpawnItemView::Definition);
    registry.Add(SpawnObjectInfoView::Definition);
}
