/*
 * Project Ambrose by Imjustchico
 * An instance of any property class from the loaded type dump: values stored by property ordinal, read and written by name, hash or ordinal, with list elements and child objects edited in place, every write checked against the property's kind, container, bit width, class, catalog and ownership and taking the offered value only when it succeeds, built with the dump's defaults, deep-cloned and compared exactly, and keeping the catalog it was built from alive, with blank construction and direct value access reserved for the serializers that fill every value themselves.
 */

#ifndef AMBROSE_PROPERTYOBJECT_H
#define AMBROSE_PROPERTYOBJECT_H

#include "PropertyValue.h"
#include "TypeRegistry.h"

#include <cstddef>
#include <string_view>
#include <vector>

enum class PropertySetResult : uint8
{
    Ok,
    UnknownProperty,
    WrongKind,
    OutOfRange,
    WrongClass,
    OtherCatalog,
    NullNotAllowed,
    NoSuchElement,
    WouldOwnItself
};

class PropertyObject
{
public:
    class BuildKey
    {
    public:
        BuildKey(BuildKey const&) noexcept
        {
        }

    private:
        friend class ObjectSerializer;

        BuildKey() noexcept
        {
        }
    };

    PropertyObject(PropertyObject const&) = delete;
    PropertyObject& operator=(PropertyObject const&) = delete;

    static PropertyObjectPtr Create(TypeCatalogPtr catalog, ClassInfo const& type);
    static PropertyObjectPtr Create(TypeCatalogPtr catalog, std::string_view className);
    static PropertyObjectPtr CreateBlank(BuildKey key, TypeCatalogPtr const& catalog, ClassInfo const& type);
    static PropertyValue MakeDefault(TypeCatalogPtr const& catalog, PropertyInfo const& property);
    static PropertySetResult Check(PropertyInfo const& property, PropertyValue const& value) noexcept;
    static std::size_t StorageIndexOf(PropertyInfo const& property) noexcept;
    static std::string_view GetResultName(PropertySetResult result) noexcept;

    ClassInfo const& GetClass() const noexcept { return *_type; }
    TypeCatalogPtr const& GetCatalog() const noexcept { return _catalog; }
    bool IsA(ClassInfo const& type) const noexcept { return _type->IsA(type); }
    bool IsA(std::string_view className) const noexcept;

    PropertyValue const* Get(std::string_view name) const noexcept;
    PropertyValue const* Get(uint32 hash) const noexcept;
    PropertyValue const* GetAt(std::size_t ordinal) const noexcept;

    PropertySetResult Set(std::string_view name, PropertyValue&& value);
    PropertySetResult Set(uint32 hash, PropertyValue&& value);
    PropertySetResult SetAt(std::size_t ordinal, PropertyValue&& value);
    PropertySetResult SetElementAt(std::size_t ordinal, std::size_t index, PropertyValue&& value);
    PropertySetResult EraseElementAt(std::size_t ordinal, std::size_t index);
    PropertyObject* EditObjectAt(std::size_t ordinal, std::size_t index = 0) noexcept;

    PropertyObjectPtr Clone() const;
    bool operator==(PropertyObject const& other) const;

    std::vector<PropertyValue>& GetValues(BuildKey key) noexcept;
    bool IsPresent(std::size_t ordinal) const noexcept;
    void MarkPresent(std::size_t ordinal, BuildKey key) noexcept;
    std::vector<std::size_t> const& GetPresentOrder() const noexcept { return _presentOrder; }
    bool HasPreservedOrder() const noexcept { return _preserveOrder; }

private:
    PropertyObject(TypeCatalogPtr catalog, ClassInfo const& type) noexcept;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, ClassInfo const& type);
    bool Reaches(PropertyValue const& value) const noexcept;

    TypeCatalogPtr _catalog;
    ClassInfo const* _type;
    std::vector<PropertyValue> _values;
    std::vector<bool> _present;
    std::vector<std::size_t> _presentOrder;
    bool _preserveOrder = false;
};

#endif
