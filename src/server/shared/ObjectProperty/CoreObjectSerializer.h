/*
 * Project Ambrose by Imjustchico
 * The CoreObject form a game object travels in inside MSG_LOGINCOMPLETE and MSG_NEWOBJECT: the table of which block and type stand for which class, built off to the side and checked against the type catalog it will serve, so that every class resolves, is a CoreObject and holds one pair while no pair repeats and none is the plain pair that means a class hash follows, and the calls that encode and decode an object or a message field in that form with the table bound in.
 */

#ifndef AMBROSE_COREOBJECTSERIALIZER_H
#define AMBROSE_COREOBJECTSERIALIZER_H

#include "ObjectSerializer.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct CoreObjectType
{
    uint8 Block = 0;
    uint8 Type = 0;
    std::string ClassName;
    uint32 ClassHash = 0;
};

class CoreObjectTypeTable
{
public:
    static constexpr std::string_view CoreObjectClass = "class CoreObject";

    CoreObjectTypeTable() = default;

    static std::shared_ptr<CoreObjectTypeTable const> Build(std::vector<CoreObjectType> types, TypeCatalog const& catalog, std::vector<std::string>& errors);

    CoreObjectType const* Find(uint8 block, uint8 type) const noexcept;
    CoreObjectType const* FindByClass(uint32 classHash) const noexcept;
    std::span<CoreObjectType const> GetTypes() const noexcept { return _types; }
    std::size_t Count() const noexcept { return _types.size(); }

private:
    static uint16 PairKey(uint8 block, uint8 type) noexcept { return static_cast<uint16>((uint16{ block } << 8) | type); }

    std::vector<CoreObjectType> _types;
    std::unordered_map<uint16, std::size_t> _byPair;
    std::unordered_map<uint32, std::size_t> _byClass;
};

using CoreObjectTypeTablePtr = std::shared_ptr<CoreObjectTypeTable const>;

namespace CoreObjectSerializer
{
    EncodeResult Encode(PropertyObject const& object, CoreObjectTypeTable const& types, SerializerOptions options = {});
    DecodeResult Decode(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, CoreObjectTypeTable const& types, SerializerOptions options = {});
    EncodeResult EncodeField(ObjectField const& field, PropertyObject const& object, CoreObjectTypeTable const& types, SerializerOptions options = {});
    DecodeResult DecodeField(TypeCatalogPtr const& catalog, ObjectField const& field, std::span<uint8 const> bytes, CoreObjectTypeTable const& types, SerializerOptions options = {});
}

#endif
