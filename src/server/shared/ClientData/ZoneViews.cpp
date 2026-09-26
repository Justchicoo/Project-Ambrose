/*
 * Project Ambrose by Imjustchico
 * Adds the zone data, location and object info views to a view registry before its type dump loads.
 */

#include "ZoneViews.h"

void ZoneViews::RegisterAll(TypedViewRegistry& registry)
{
    registry.Add(WizZoneDataView::Definition);
    registry.Add(LocationTemplateView::Definition);
    registry.Add(CoreObjectInfoView::Definition);
}
