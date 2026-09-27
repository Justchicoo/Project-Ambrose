/*
 * Project Ambrose by Imjustchico
 * The CoreObject form a game object travels in inside MSG_LOGINCOMPLETE and MSG_NEWOBJECT, whose header is a core type, the type of the object's template and the template id: the table of which class each core type builds, as the client's own factory registers them, and of which core type and template type each template class gives, built off to the side and checked against the type catalog it will serve, so that every class resolves and is a CoreObject, every template class resolves and is a CoreTemplate whose core type the table builds, and no core type or template class repeats; and the calls that encode and decode an object or a message field in that form with the table bound in.
 */

#ifndef AMBROSE_COREOBJECTSERIALIZER_H
#define AMBROSE_COREOBJECTSERIALIZER_H

#include "ObjectSerializer.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct CoreObjectType
{
    uint8 CoreType = 0;
    std::string ClassName;
    uint32 ClassHash = 0;
};

struct CoreTemplateType
{
    std::string TemplateClass;
    uint8 CoreType = 0;
    uint8 TemplateType = 0;
    uint32 TemplateClassHash = 0;
};

class CoreObjectTypeTable
{
public:
    static constexpr std::string_view CoreObjectClass = "class CoreObject";
    static constexpr std::string_view CoreTemplateClass = "class CoreTemplate";

    CoreObjectTypeTable() = default;

    static std::shared_ptr<CoreObjectTypeTable const> Build(std::vector<CoreObjectType> types, std::vector<CoreTemplateType> templates, TypeCatalog const& catalog,
        std::vector<std::string>& errors);

    CoreObjectType const* Find(uint8 coreType) const noexcept;
    bool Builds(uint8 coreType, uint32 classHash) const noexcept;
    bool IsCoreClass(uint32 classHash) const noexcept;
    CoreTemplateType const* FindTemplate(ClassInfo const& templateClass) const noexcept;
    bool IsTemplatePair(uint8 coreType, uint8 templateType) const noexcept;
    std::optional<CoreObjectHeader> HeaderFor(ClassInfo const& templateClass, uint32 templateId) const noexcept;
    std::span<CoreObjectType const> GetTypes() const noexcept { return _types; }
    std::span<CoreTemplateType const> GetTemplates() const noexcept { return _templates; }
    std::size_t Count() const noexcept { return _types.size(); }

private:
    std::vector<CoreObjectType> _types;
    std::vector<CoreTemplateType> _templates;
    std::unordered_map<uint8, std::size_t> _byCoreType;
    std::unordered_map<uint32, std::size_t> _byTemplateClass;
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
