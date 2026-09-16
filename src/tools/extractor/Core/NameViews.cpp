/*
 * Project Ambrose by Imjustchico
 * Adds the disallowed name views to a view registry before its type dump loads.
 */

#include "NameViews.h"

void NameViews::RegisterAll(TypedViewRegistry& registry)
{
    registry.Add(DisallowedNameListView::Definition);
    registry.Add(DisallowedNameView::Definition);
}
