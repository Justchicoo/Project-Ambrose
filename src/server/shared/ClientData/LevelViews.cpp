/*
 * Project Ambrose by Imjustchico
 * Adds the level, school and stat views to a view registry before its type dump loads.
 */

#include "LevelViews.h"

void LevelViews::RegisterAll(TypedViewRegistry& registry)
{
    registry.Add(MagicXPConfigView::Definition);
    registry.Add(MagicLevelInfoView::Definition);
    registry.Add(MobRankLevelView::Definition);
    registry.Add(SchoolLevelTableView::Definition);
    registry.Add(MagicSchoolTemplateView::Definition);
    registry.Add(StatEffectConfigView::Definition);
    registry.Add(CritAndBlockBandView::Definition);
    registry.Add(CritAndBlockValuesView::Definition);
    registry.Add(PipConversionBandView::Definition);
    registry.Add(PipConversionValuesView::Definition);
}
