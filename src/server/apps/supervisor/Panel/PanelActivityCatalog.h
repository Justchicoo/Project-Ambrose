/*
 * Project Ambrose by Imjustchico
 * One sentence for every event the panel can emit, in namespace:path.action form with escaped values, so the activity pages read like a log instead of a table of codes, and a test fails when the code emits an event the catalog has no sentence for.
 */

#ifndef AMBROSE_PANELACTIVITYCATALOG_H
#define AMBROSE_PANELACTIVITYCATALOG_H

#include "Types.h"

#include <string>
#include <string_view>
#include <vector>

struct AuditEvent;

namespace PanelActivityCatalog
{
    struct Sentence
    {
        std::string_view Name;
        std::string_view Text;
    };

    std::vector<Sentence> const& All();
    bool Has(std::string_view name) noexcept;
    std::string Render(std::string_view name, AuditEvent const& event);
    std::string Escape(std::string_view value);
}

#endif
