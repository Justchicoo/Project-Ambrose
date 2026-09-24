/*
 * Project Ambrose by Imjustchico
 * The typed views the extractor reads the client's disallowed name list through: the list and each disallowed name's locale, gender and first, middle and last indices.
 */

#ifndef AMBROSE_NAMEVIEWS_H
#define AMBROSE_NAMEVIEWS_H

#include "TypedView.h"

#include <array>
#include <cstddef>

class DisallowedNameListView : public TypedView<DisallowedNameListView>
{
public:
    enum Field : std::size_t { Names, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(Names, "class SharedPointer<class DisallowedName>", "m_disallowedNameList"),
    } };
    static constexpr ViewDefinition Definition{ "DisallowedNameListView", "class DisallowedNameList", Fields };

    decltype(auto) GetNames() const noexcept { return Read<Names>(); }

    AMBROSE_TYPED_VIEW(DisallowedNameListView)
};

class DisallowedNameView : public TypedView<DisallowedNameView>
{
public:
    enum Field : std::size_t { Locale, Gender, First, Middle, Last, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint32>(Locale, "unsigned int", "m_locale"),
        ViewField::Of<uint32>(Gender, "unsigned int", "m_gender"),
        ViewField::Of<uint32>(First, "unsigned int", "m_first"),
        ViewField::Of<uint32>(Middle, "unsigned int", "m_middle"),
        ViewField::Of<uint32>(Last, "unsigned int", "m_last"),
    } };
    static constexpr ViewDefinition Definition{ "DisallowedNameView", "class DisallowedName", Fields };

    decltype(auto) GetLocale() const noexcept { return Read<Locale>(); }
    decltype(auto) GetGender() const noexcept { return Read<Gender>(); }
    decltype(auto) GetFirst() const noexcept { return Read<First>(); }
    decltype(auto) GetMiddle() const noexcept { return Read<Middle>(); }
    decltype(auto) GetLast() const noexcept { return Read<Last>(); }

    AMBROSE_TYPED_VIEW(DisallowedNameView)
};

namespace NameViews
{
    void RegisterAll(TypedViewRegistry& registry);
}

#endif
