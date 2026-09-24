/*
 * Project Ambrose by Imjustchico
 * Compile-checked views over dynamic property objects: the registry of views every type dump load binds or refuses, closed to new views once a dump has loaded, and the base a view derives from, built over an object only when the object's catalog bound the view and the object is of its class, reading each field as the storage type it declared through ordinals cached once.
 */

#ifndef AMBROSE_TYPEDVIEW_H
#define AMBROSE_TYPEDVIEW_H

#include "PropertyObject.h"
#include "ViewDefinition.h"

#include <cstddef>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

class TypedViewRegistry
{
public:
    TypedViewRegistry() = default;
    TypedViewRegistry(TypedViewRegistry const&) = delete;
    TypedViewRegistry& operator=(TypedViewRegistry const&) = delete;

    static TypedViewRegistry& Instance();

    bool Add(ViewDefinition const& definition);
    std::vector<ViewDefinition const*> GetViews() const;
    std::vector<ViewDefinition const*> Seal();
    bool IsSealed() const;

    static bool Bind(TypeCatalog const& catalog, ViewDefinition const& definition, ViewBinding& binding, std::vector<std::string>& errors);

private:
    mutable std::mutex _mutex;
    std::vector<ViewDefinition const*> _views;
    bool _sealed = false;
};

#define sTypedViewRegistry TypedViewRegistry::Instance()

#define AMBROSE_TYPED_VIEW(View) \
private: \
    friend class TypedView<View>; \
    View(PropertyObject const& object, uint32 const* ordinals) noexcept : TypedView<View>(object, ordinals) \
    { \
    }

template<typename Derived>
class TypedView
{
public:
    static std::optional<Derived> From(PropertyObject const& object) noexcept
    {
        static_assert(FieldsAreInOrder(), "a view's Fields must list each field at the position of its enum value");
        TypeCatalog const* const catalog = object.GetClass().Owner;
        ViewBinding const* const binding = catalog ? catalog->FindView(Derived::Definition) : nullptr;
        if (!binding || !object.IsA(*binding->Class))
            return std::nullopt;
        return Derived(object, binding->Ordinals.data());
    }

    static std::optional<Derived> From(PropertyObject const* object) noexcept
    {
        return object ? From(*object) : std::nullopt;
    }

    PropertyObject const& Target() const noexcept { return *_object; }

    uint32 GetOrdinal(std::size_t field) const noexcept
    {
        return field < Derived::Fields.size() ? _ordinals[field] : std::numeric_limits<uint32>::max();
    }

protected:
    TypedView(PropertyObject const& object, uint32 const* ordinals) noexcept : _object(&object), _ordinals(ordinals)
    {
    }

    template<std::size_t Field>
    decltype(auto) Read() const noexcept
    {
        using Stored = std::variant_alternative_t<Derived::Fields[Field].StorageIndex, PropertyValue::Storage>;
        PropertyValue const& value = *_object->GetAt(_ordinals[Field]);
        if constexpr (std::is_same_v<Stored, PropertyObjectPtr>)
            return value.AsObject();
        else if constexpr (std::is_same_v<Stored, PropertyValue::List>)
            return *value.GetList();
        else
            return *value.template GetIf<Stored>();
    }

private:
    static constexpr bool FieldsAreInOrder() noexcept
    {
        for (std::size_t index = 0; index < Derived::Fields.size(); ++index)
            if (Derived::Fields[index].Index != index)
                return false;
        return true;
    }

    PropertyObject const* _object;
    uint32 const* _ordinals;
};

#endif
