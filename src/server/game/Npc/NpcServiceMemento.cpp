/*
 * Project Ambrose by Imjustchico
 * Turns an NPC's menu and presentation into the ServiceMementoBase MSG_SENDNPCOPTIONS carries, a quest offered as a PrepEntry and any other option as an InteractableOption, and encodes it as the bare object, as the client reads its first four bytes as the class hash and a BlobEnvelope header fails it with "Failed to find type by name hash".
 */

#include "NpcServiceMemento.h"

#include <utility>

QuestWireEncoder::ServiceMementoBase NpcServiceMemento::Build(NpcServiceMenu const& menu, NpcPresentation const& npc)
{
    QuestWireEncoder::ServiceMementoBase memento;
    std::vector<NpcServiceMenu::Entry> const& entries = menu.GetEntries();
    for (uint32 index = 0; index < entries.size(); ++index)
    {
        if (entries[index].Option.PrepText)
        {
            QuestWireEncoder::PrepEntry prep;
            prep.ServiceName = entries[index].Option.ServiceName;
            prep.IconKey = entries[index].Option.IconKey;
            prep.DisplayKey = entries[index].Option.DisplayKey;
            prep.ServiceIndex = index;
            prep.PrepText = *entries[index].Option.PrepText;
            memento.Options.emplace_back(std::move(prep));
            continue;
        }
        QuestWireEncoder::InteractableOption option;
        option.ServiceName = entries[index].Option.ServiceName;
        option.IconKey = entries[index].Option.IconKey;
        option.DisplayKey = entries[index].Option.DisplayKey;
        option.ServiceIndex = index;
        option.OptionIndex = static_cast<int32>(index);
        memento.Options.emplace_back(std::move(option));
    }
    QuestMadlibs::NpcFields fields;
    fields.Name = npc.DisplayKey;
    memento.PersonaMadlibs = QuestMadlibs::BuildNpc(fields);
    memento.NpcNameKey = menu.GetNameKey().value_or(std::string(DefaultNameKey));
    memento.NpcTextKey = menu.GetTextKey().value_or(std::string(DefaultTextKey));
    memento.NpcIcon = menu.GetIcon().value_or(npc.Icon);
    return memento;
}

QuestWireEncoder::BlobEncodeResult NpcServiceMemento::Encode(TypeCatalogPtr const& catalog, NpcServiceMenu const& menu, NpcPresentation const& npc)
{
    QuestWireEncoder::ObjectBuildResult built = QuestWireEncoder::BuildServiceMementoBase(catalog, Build(menu, npc));
    if (!built.Ok())
    {
        QuestWireEncoder::BlobEncodeResult failed;
        failed.Error = built.Error.empty() ? std::string("the service memento could not be built") : std::move(built.Error);
        return failed;
    }
    return QuestWireEncoder::EncodeBare(*built.Object, QuestWireEncoder::ServiceMementoMask);
}
