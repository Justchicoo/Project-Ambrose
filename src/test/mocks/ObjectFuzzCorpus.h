/*
 * Project Ambrose by Imjustchico
 * A small type catalog of invented classes covering every value layout, list, pointer, inline and derived object the codec handles, seed inputs made of a mode byte and a golden blob, plain, with text enums, with compact lengths, versionable, or inside a stored or compressed envelope, and the decode and round-trip check shared by the decoder fuzz test and the libFuzzer target.
 */

#ifndef AMBROSE_OBJECTFUZZCORPUS_H
#define AMBROSE_OBJECTFUZZCORPUS_H

#include "ObjectSerializer.h"

#include <span>
#include <string>
#include <vector>

namespace ObjectFuzzCorpus
{
    inline constexpr uint8 TextEnums = 1;
    inline constexpr uint8 AllowTrailing = 2;
    inline constexpr uint8 Enveloped = 4;
    inline constexpr uint8 Versionable = 8;
    inline constexpr uint8 CompactLengths = 16;
    inline constexpr uint8 ModeBits = 5;

    struct Seed
    {
        uint8 Mode = 0;
        std::vector<uint8> Bytes;
    };

    TypeCatalogPtr LoadCatalog(std::string& error);
    std::vector<Seed> MakeSeeds(TypeCatalogPtr const& catalog);
    ObjectField const& GetEnvelopedField() noexcept;
    SerializerOptions MakeOptions(uint8 mode);
    DecodeResult Decode(TypeCatalogPtr const& catalog, uint8 mode, std::span<uint8 const> bytes);
    std::string CheckDecoded(TypeCatalogPtr const& catalog, uint8 mode, std::span<uint8 const> bytes, DecodeResult const& decoded);
}

#endif
