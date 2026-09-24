/*
 * Project Ambrose by Imjustchico
 * Builds property objects from the defaults the type registry resolved at load, inline child objects created and pointers left null, checks every write, whole or one list element at a time, against the property's value kind, container, bit width, enum range, nullability, class and catalog, refuses a write that would make an object own itself, leaving a refused value with the caller, hands out child objects for in-place edits, and clones and compares objects deeply.
 */

#include "PropertyObject.h"

#include <cstddef>
#include <limits>

namespace
{
    bool FitsSigned(int64 value, uint8 bits) noexcept
    {
        if (bits == 0 || bits >= 64)
            return true;
        int64 const limit = int64{ 1 } << (bits - 1);
        return value >= -limit && value < limit;
    }

    bool FitsUnsigned(uint64 value, uint8 bits) noexcept
    {
        return bits == 0 || bits >= 64 || value < (uint64{ 1 } << bits);
    }

    PropertySetResult Result(bool matches) noexcept
    {
        return matches ? PropertySetResult::Ok : PropertySetResult::WrongKind;
    }

    PropertySetResult CheckElement(PropertyInfo const& property, PropertyValue const& value) noexcept
    {
        switch (property.Kind)
        {
            case ValueKind::Bool: return Result(value.Holds<bool>());
            case ValueKind::Int8: return Result(value.Holds<int8>());
            case ValueKind::UInt8: return Result(value.Holds<uint8>());
            case ValueKind::Int16: return Result(value.Holds<int16>());
            case ValueKind::UInt16: return Result(value.Holds<uint16>());
            case ValueKind::Int32: return Result(value.Holds<int32>());
            case ValueKind::UInt32: return Result(value.Holds<uint32>());
            case ValueKind::Int64: return Result(value.Holds<int64>());
            case ValueKind::UInt64:
            case ValueKind::Gid: return Result(value.Holds<uint64>());
            case ValueKind::Float: return Result(value.Holds<float>());
            case ValueKind::Double: return Result(value.Holds<double>());
            case ValueKind::WideChar: return Result(value.Holds<char16_t>());
            case ValueKind::String: return Result(value.Holds<std::string>());
            case ValueKind::WideString: return Result(value.Holds<std::u16string>());
            case ValueKind::Enum:
            {
                int64 const* const number = value.GetIf<int64>();
                if (!number)
                    return PropertySetResult::WrongKind;
                return *number >= 0 && *number <= static_cast<int64>(std::numeric_limits<uint32>::max()) ? PropertySetResult::Ok : PropertySetResult::OutOfRange;
            }
            case ValueKind::SignedBits:
            case ValueKind::S24:
            {
                int32 const* const number = value.GetIf<int32>();
                if (!number)
                    return PropertySetResult::WrongKind;
                return FitsSigned(*number, property.Kind == ValueKind::S24 ? uint8{ 24 } : property.BitWidth) ? PropertySetResult::Ok : PropertySetResult::OutOfRange;
            }
            case ValueKind::UnsignedBits:
            case ValueKind::U24:
            {
                uint32 const* const number = value.GetIf<uint32>();
                if (!number)
                    return PropertySetResult::WrongKind;
                return FitsUnsigned(*number, property.Kind == ValueKind::U24 ? uint8{ 24 } : property.BitWidth) ? PropertySetResult::Ok : PropertySetResult::OutOfRange;
            }
            case ValueKind::Object:
            {
                if (!value.Holds<PropertyObjectPtr>())
                    return PropertySetResult::WrongKind;
                PropertyObject const* const object = value.AsObject();
                if (!object)
                    return property.Pointer ? PropertySetResult::Ok : PropertySetResult::NullNotAllowed;
                if (!property.Type)
                    return PropertySetResult::WrongClass;
                if (object->GetClass().Owner != property.Type->Owner)
                    return PropertySetResult::OtherCatalog;
                return object->IsA(*property.Type) ? PropertySetResult::Ok : PropertySetResult::WrongClass;
            }
            case ValueKind::Vector3D: return Result(value.Holds<PropertyTypes::Vector3D>());
            case ValueKind::Quaternion: return Result(value.Holds<PropertyTypes::Quaternion>());
            case ValueKind::Matrix3x3: return Result(value.Holds<PropertyTypes::Matrix3x3>());
            case ValueKind::Euler: return Result(value.Holds<PropertyTypes::Euler>());
            case ValueKind::Color: return Result(value.Holds<PropertyTypes::Color>());
            case ValueKind::PointInt: return Result(value.Holds<PropertyTypes::PointInt>());
            case ValueKind::PointFloat: return Result(value.Holds<PropertyTypes::PointFloat>());
            case ValueKind::SizeInt: return Result(value.Holds<PropertyTypes::SizeInt>());
            case ValueKind::RectInt: return Result(value.Holds<PropertyTypes::RectInt>());
            case ValueKind::RectFloat: return Result(value.Holds<PropertyTypes::RectFloat>());
            case ValueKind::SerializedBuffer: return Result(value.Holds<PropertyTypes::SerializedBuffer>());
            case ValueKind::SimpleVert: return Result(value.Holds<PropertyTypes::SimpleVert>());
            case ValueKind::SimpleFace: return Result(value.Holds<PropertyTypes::SimpleFace>());
        }
        return PropertySetResult::WrongKind;
    }
}

PropertyObject::PropertyObject(TypeCatalogPtr catalog, ClassInfo const& type) noexcept : _catalog(std::move(catalog)), _type(&type)
{
}

PropertyObjectPtr PropertyObject::Create(TypeCatalogPtr catalog, ClassInfo const& type)
{
    if (!catalog || type.Kind != ClassKind::PropertyClass || type.Owner != catalog.get())
        return nullptr;
    return Build(catalog, type);
}

PropertyObjectPtr PropertyObject::Create(TypeCatalogPtr catalog, std::string_view className)
{
    if (!catalog)
        return nullptr;
    ClassInfo const* const type = catalog->FindClass(className);
    return type ? Create(std::move(catalog), *type) : nullptr;
}

PropertyObjectPtr PropertyObject::Build(TypeCatalogPtr const& catalog, ClassInfo const& type)
{
    PropertyObjectPtr object(new PropertyObject(catalog, type));
    object->_values.reserve(type.Properties.size());
    object->_present.resize(type.Properties.size());
    for (PropertyInfo const& property : type.Properties)
        object->_values.push_back(MakeDefault(catalog, property));
    return object;
}

PropertyObjectPtr PropertyObject::CreateBlank(BuildKey, TypeCatalogPtr const& catalog, ClassInfo const& type)
{
    PropertyObjectPtr object(new PropertyObject(catalog, type));
    object->_values.resize(type.Properties.size());
    object->_present.resize(type.Properties.size());
    object->_preserveOrder = true;
    return object;
}

PropertyValue PropertyObject::MakeDefault(TypeCatalogPtr const& catalog, PropertyInfo const& property)
{
    if (property.Container != ContainerKind::Static)
        return PropertyValue::List();
    if (property.Kind != ValueKind::Object)
        return property.DefaultValue;
    if (property.Pointer || !property.Type || property.Type->Kind != ClassKind::PropertyClass || !catalog || property.Type->Owner != catalog.get())
        return PropertyObjectPtr();
    return Build(catalog, *property.Type);
}

PropertySetResult PropertyObject::Check(PropertyInfo const& property, PropertyValue const& value) noexcept
{
    if (property.Container == ContainerKind::Static)
        return CheckElement(property, value);
    PropertyValue::List const* const list = value.GetList();
    if (!list)
        return PropertySetResult::WrongKind;
    for (PropertyValue const& element : *list)
        if (PropertySetResult const result = CheckElement(property, element); result != PropertySetResult::Ok)
            return result;
    return PropertySetResult::Ok;
}

std::size_t PropertyObject::StorageIndexOf(PropertyInfo const& property) noexcept
{
    if (property.Container != ContainerKind::Static)
        return PropertyValue::IndexOf<PropertyValue::List>();
    switch (property.Kind)
    {
        case ValueKind::Bool: return PropertyValue::IndexOf<bool>();
        case ValueKind::Int8: return PropertyValue::IndexOf<int8>();
        case ValueKind::UInt8: return PropertyValue::IndexOf<uint8>();
        case ValueKind::Int16: return PropertyValue::IndexOf<int16>();
        case ValueKind::UInt16: return PropertyValue::IndexOf<uint16>();
        case ValueKind::Int32:
        case ValueKind::SignedBits:
        case ValueKind::S24: return PropertyValue::IndexOf<int32>();
        case ValueKind::UInt32:
        case ValueKind::UnsignedBits:
        case ValueKind::U24: return PropertyValue::IndexOf<uint32>();
        case ValueKind::Int64:
        case ValueKind::Enum: return PropertyValue::IndexOf<int64>();
        case ValueKind::UInt64:
        case ValueKind::Gid: return PropertyValue::IndexOf<uint64>();
        case ValueKind::Float: return PropertyValue::IndexOf<float>();
        case ValueKind::Double: return PropertyValue::IndexOf<double>();
        case ValueKind::WideChar: return PropertyValue::IndexOf<char16_t>();
        case ValueKind::String: return PropertyValue::IndexOf<std::string>();
        case ValueKind::WideString: return PropertyValue::IndexOf<std::u16string>();
        case ValueKind::Object: return PropertyValue::IndexOf<PropertyObjectPtr>();
        case ValueKind::Vector3D: return PropertyValue::IndexOf<PropertyTypes::Vector3D>();
        case ValueKind::Quaternion: return PropertyValue::IndexOf<PropertyTypes::Quaternion>();
        case ValueKind::Matrix3x3: return PropertyValue::IndexOf<PropertyTypes::Matrix3x3>();
        case ValueKind::Euler: return PropertyValue::IndexOf<PropertyTypes::Euler>();
        case ValueKind::Color: return PropertyValue::IndexOf<PropertyTypes::Color>();
        case ValueKind::PointInt: return PropertyValue::IndexOf<PropertyTypes::PointInt>();
        case ValueKind::PointFloat: return PropertyValue::IndexOf<PropertyTypes::PointFloat>();
        case ValueKind::SizeInt: return PropertyValue::IndexOf<PropertyTypes::SizeInt>();
        case ValueKind::RectInt: return PropertyValue::IndexOf<PropertyTypes::RectInt>();
        case ValueKind::RectFloat: return PropertyValue::IndexOf<PropertyTypes::RectFloat>();
        case ValueKind::SerializedBuffer: return PropertyValue::IndexOf<PropertyTypes::SerializedBuffer>();
        case ValueKind::SimpleVert: return PropertyValue::IndexOf<PropertyTypes::SimpleVert>();
        case ValueKind::SimpleFace: return PropertyValue::IndexOf<PropertyTypes::SimpleFace>();
    }
    return std::variant_npos;
}

std::string_view PropertyObject::GetResultName(PropertySetResult result) noexcept
{
    switch (result)
    {
        case PropertySetResult::Ok: return "ok";
        case PropertySetResult::UnknownProperty: return "the class has no such property";
        case PropertySetResult::WrongKind: return "the value is not of the property's kind or container";
        case PropertySetResult::OutOfRange: return "the value does not fit the property's bit width or 32-bit enum range";
        case PropertySetResult::WrongClass: return "the object is not of the property's class";
        case PropertySetResult::OtherCatalog: return "the object was built from another type catalog generation than its parent";
        case PropertySetResult::NullNotAllowed: return "an inline object property cannot be null";
        case PropertySetResult::NoSuchElement: return "the index is past the end of the list";
        case PropertySetResult::WouldOwnItself: return "the object would end up owning itself";
    }
    return "unknown";
}

bool PropertyObject::IsA(std::string_view className) const noexcept
{
    ClassInfo const* const type = _catalog->FindClass(className);
    return type && _type->IsA(*type);
}

PropertyValue const* PropertyObject::Get(std::string_view name) const noexcept
{
    PropertyInfo const* const property = _type->FindProperty(name);
    return property ? &_values[property->Id] : nullptr;
}

PropertyValue const* PropertyObject::Get(uint32 hash) const noexcept
{
    PropertyInfo const* const property = _type->FindProperty(hash);
    return property ? &_values[property->Id] : nullptr;
}

PropertyValue const* PropertyObject::GetAt(std::size_t ordinal) const noexcept
{
    return ordinal < _values.size() ? &_values[ordinal] : nullptr;
}

PropertySetResult PropertyObject::Set(std::string_view name, PropertyValue&& value)
{
    PropertyInfo const* const property = _type->FindProperty(name);
    return property ? SetAt(property->Id, std::move(value)) : PropertySetResult::UnknownProperty;
}

PropertySetResult PropertyObject::Set(uint32 hash, PropertyValue&& value)
{
    PropertyInfo const* const property = _type->FindProperty(hash);
    return property ? SetAt(property->Id, std::move(value)) : PropertySetResult::UnknownProperty;
}

PropertySetResult PropertyObject::SetAt(std::size_t ordinal, PropertyValue&& value)
{
    if (ordinal >= _values.size())
        return PropertySetResult::UnknownProperty;
    PropertyInfo const& property = _type->Properties[ordinal];
    if (PropertySetResult const result = Check(property, value); result != PropertySetResult::Ok)
        return result;
    if (property.Kind == ValueKind::Object && Reaches(value))
        return PropertySetResult::WouldOwnItself;
    _values[ordinal] = std::move(value);
    _present[ordinal] = true;
    return PropertySetResult::Ok;
}

PropertySetResult PropertyObject::SetElementAt(std::size_t ordinal, std::size_t index, PropertyValue&& value)
{
    if (ordinal >= _values.size())
        return PropertySetResult::UnknownProperty;
    PropertyValue::List* const list = _values[ordinal].GetIf<PropertyValue::List>();
    if (!list)
        return PropertySetResult::WrongKind;
    if (index > list->size())
        return PropertySetResult::NoSuchElement;
    PropertyInfo const& property = _type->Properties[ordinal];
    if (PropertySetResult const result = CheckElement(property, value); result != PropertySetResult::Ok)
        return result;
    if (property.Kind == ValueKind::Object && Reaches(value))
        return PropertySetResult::WouldOwnItself;
    if (index == list->size())
        list->push_back(std::move(value));
    else
        (*list)[index] = std::move(value);
    return PropertySetResult::Ok;
}

PropertySetResult PropertyObject::EraseElementAt(std::size_t ordinal, std::size_t index)
{
    if (ordinal >= _values.size())
        return PropertySetResult::UnknownProperty;
    PropertyValue::List* const list = _values[ordinal].GetIf<PropertyValue::List>();
    if (!list)
        return PropertySetResult::WrongKind;
    if (index >= list->size())
        return PropertySetResult::NoSuchElement;
    list->erase(list->begin() + static_cast<std::ptrdiff_t>(index));
    return PropertySetResult::Ok;
}

PropertyObject* PropertyObject::EditObjectAt(std::size_t ordinal, std::size_t index) noexcept
{
    if (ordinal >= _values.size() || _type->Properties[ordinal].Kind != ValueKind::Object)
        return nullptr;
    if (PropertyValue::List* const list = _values[ordinal].GetIf<PropertyValue::List>())
        return index < list->size() ? (*list)[index].AsObject() : nullptr;
    return index == 0 ? _values[ordinal].AsObject() : nullptr;
}

PropertyObjectPtr PropertyObject::Clone() const
{
    PropertyObjectPtr copy(new PropertyObject(_catalog, *_type));
    copy->_values = _values;
    copy->_present = _present;
    copy->_presentOrder = _presentOrder;
    copy->_preserveOrder = _preserveOrder;
    return copy;
}

bool PropertyObject::operator==(PropertyObject const& other) const
{
    return _type == other._type && _values == other._values;
}

std::vector<PropertyValue>& PropertyObject::GetValues(BuildKey) noexcept
{
    return _values;
}

bool PropertyObject::IsPresent(std::size_t ordinal) const noexcept
{
    return ordinal < _present.size() && _present[ordinal];
}

void PropertyObject::MarkPresent(std::size_t ordinal, BuildKey) noexcept
{
    if (ordinal < _present.size() && !_present[ordinal])
    {
        _present[ordinal] = true;
        _presentOrder.push_back(ordinal);
    }
}

bool PropertyObject::Reaches(PropertyValue const& value) const noexcept
{
    if (PropertyValue::List const* const list = value.GetList())
    {
        for (PropertyValue const& element : *list)
            if (Reaches(element))
                return true;
        return false;
    }
    PropertyObject const* const object = value.AsObject();
    if (!object)
        return false;
    if (object == this)
        return true;
    for (std::size_t ordinal = 0; ordinal < object->_values.size(); ++ordinal)
        if (object->_type->Properties[ordinal].Kind == ValueKind::Object && Reaches(object->_values[ordinal]))
            return true;
    return false;
}
