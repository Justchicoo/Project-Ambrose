/*
 * Project Ambrose by Imjustchico
 * A wizard's name as the client's name codec packs it into the messages that name a speaker, such as MSG_RADIALCHAT's SourceName, read least significant bit first: for a name of table parts, a clear override bit, the gender whose first-name table the first index counts in, 0 female and 1 male, the five-bit locale whose tables the reader counts the indices in, which the client's own PackName always writes as 0, the reader's own, and so does FromIndices, since the locale a wizard's stored indices carry counts the locales in another order, a set bit, then the first, middle and last indices, four bytes in all; for a name no table holds, a set override bit, a clear bit that says UTF-16 text follows, its length in bytes in five bits, a set bit, then the text. ForWizard packs a wizard's name the way the server names it to other clients, from its custom name when that is valid UTF-8 short enough for an override and from its name parts otherwise. Unpack reads either back.
 */

#ifndef AMBROSE_PACKEDNAME_H
#define AMBROSE_PACKEDNAME_H

#include "CharacterNames.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

struct UnpackedName
{
    bool Override = false;
    uint32 Gender = 0;
    NameIndices Indices;
    std::u16string Text;
};

class PackedName
{
public:
    static constexpr std::size_t MaxOverrideBytes = 31;
    static constexpr uint32 ReadersOwnLocale = 0;

    PackedName() = delete;

    static std::string FromIndices(uint32 packedIndices, uint32 gender);
    static std::optional<std::string> FromText(std::u16string_view text);
    static std::string ForWizard(std::optional<std::string> const& customName, uint32 packedIndices, uint32 gender);
    static std::optional<UnpackedName> Unpack(std::string_view packed);
};

#endif
