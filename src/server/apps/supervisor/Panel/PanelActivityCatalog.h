/*
 * Project Ambrose by Imjustchico
 * The event catalog: one sentence for every event name the panel records, in namespace:path.action form, with the retention class each belongs to, and the rendering that fills a sentence's placeholders from a row's actor, subjects and properties with every value cut to length and cleared of control characters.
 */

#ifndef AMBROSE_PANELACTIVITYCATALOG_H
#define AMBROSE_PANELACTIVITYCATALOG_H

#include "PanelAudit.h"
#include "Types.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

enum class PanelActivityClass : uint8
{
    Security,
    HighVolume
};

struct PanelActivitySentence
{
    std::string_view Name;
    std::string_view Text;
    PanelActivityClass Class = PanelActivityClass::Security;
};

namespace PanelActivityCatalog
{
    inline constexpr std::size_t MaxValueBytes = 120;

    std::span<PanelActivitySentence const> All();
    PanelActivitySentence const* Find(std::string_view name);
    PanelActivityClass ClassOf(std::string_view name);
    std::string_view ClassName(PanelActivityClass value) noexcept;
    std::vector<std::string> NamesOf(PanelActivityClass value);
    std::string ActorText(AuditEvent const& event);
    std::string Render(AuditEvent const& event);
}

#endif
