/*
 * Project Ambrose by Imjustchico
 * The ServiceMementoBase a wizard entering an NPC's range is sent in MSG_SENDNPCOPTIONS: each option of the NPC's menu as an InteractableOption carrying its flat index, the persona madlibs as an NPC block whose NAME is the NPC's display key, the name key NPCFormats_Name and text key GUI_NPCInteractText unless a provider sets its own, and the icon its template's m_sIcon names, encoded with the service memento mask in its SerializerBinary header.
 */

#ifndef AMBROSE_NPCSERVICEMEMENTO_H
#define AMBROSE_NPCSERVICEMEMENTO_H

#include "NpcServiceMenu.h"
#include "QuestWireEncoder.h"

#include <string>
#include <string_view>

struct NpcPresentation
{
    std::string DisplayKey;
    std::string Icon;
};

namespace NpcServiceMemento
{
    inline constexpr std::string_view DefaultNameKey = "NPCFormats_Name";
    inline constexpr std::string_view DefaultTextKey = "GUI_NPCInteractText";

    QuestWireEncoder::ServiceMementoBase Build(NpcServiceMenu const& menu, NpcPresentation const& npc);
    QuestWireEncoder::BlobEncodeResult Encode(TypeCatalogPtr const& catalog, NpcServiceMenu const& menu, NpcPresentation const& npc);
}

#endif
