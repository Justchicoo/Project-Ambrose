/*
 * Project Ambrose by Imjustchico
 * Builds client engine objects in guest memory for typeextract tests: MSVC strings, type objects, property lists with bases and properties, enum options, and std::map nodes with a nil head.
 */

#ifndef AMBROSE_GUESTOBJECTS_H
#define AMBROSE_GUESTOBJECTS_H

#include "ClientLayout.h"
#include "GuestHeap.h"
#include "Machine.h"
#include "StringHash.h"

#include <string_view>
#include <utility>
#include <vector>

class GuestObjects
{
public:
    GuestObjects(Machine& machine, GuestHeap& heap, ClientLayout layout = {}) : _machine(machine), _heap(heap), _layout(layout)
    {
    }

    void WriteString(uint64 address, std::string_view text)
    {
        if (text.size() <= _layout.StringInlineCapacity)
        {
            std::vector<uint8> bytes(text.begin(), text.end());
            bytes.resize(16, 0);
            _machine.Write(address, bytes);
        }
        else
        {
            std::vector<uint8> bytes(text.begin(), text.end());
            bytes.push_back(0);
            uint64 const buffer = _heap.Allocate(bytes.size(), true);
            _machine.Write(buffer, bytes);
            _machine.WriteU64(address, buffer);
        }
        _machine.WriteU64(address + _layout.StringSize, text.size());
        _machine.WriteU64(address + _layout.StringCapacity, std::max<uint64>(text.size(), _layout.StringInlineCapacity));
    }

    uint64 Type(std::string_view name, uint64 vtable = 0x140010000, bool pointer = false, uint64 list = 0)
    {
        uint64 const type = _heap.Allocate(0xB0, true);
        _machine.WriteU64(type, vtable);
        WriteString(type + _layout.TypeName, name);
        _machine.WriteU32(type + _layout.TypeHash, StringHash::KiStringHash(name));
        _machine.WriteU8(type + _layout.TypePointer, pointer ? 1 : 0);
        _machine.WriteU64(type + _layout.TypePropertyList, list);
        return type;
    }

    uint64 List(std::string_view name, uint64 base, bool singleton = false)
    {
        uint64 const list = _heap.Allocate(0x100, true);
        _machine.WriteU8(list + _layout.ListSingleton, singleton ? 1 : 0);
        _machine.WriteU64(list + _layout.ListBase, base);
        WriteString(list + _layout.ListName, name);
        return list;
    }

    void SetProperties(uint64 list, std::vector<uint64> const& properties)
    {
        uint64 const vector = _heap.Allocate(std::max<uint64>(properties.size(), 1) * _layout.ListEntrySize, true);
        for (std::size_t i = 0; i < properties.size(); ++i)
            _machine.WriteU64(vector + i * _layout.ListEntrySize, properties[i]);
        _machine.WriteU64(list + _layout.ListProperties, vector);
        _machine.WriteU64(list + _layout.ListProperties + 8, vector + properties.size() * _layout.ListEntrySize);
    }

    uint64 Property(uint64 type, std::string_view typeName, std::string_view name, uint32 id, uint32 offset, uint32 flags, uint64 container)
    {
        uint64 const property = _heap.Allocate(0xB0, true);
        std::vector<uint8> text(name.begin(), name.end());
        text.push_back(0);
        uint64 const nameBuffer = _heap.Allocate(text.size(), true);
        _machine.Write(nameBuffer, text);
        _machine.WriteU64(property + _layout.PropertyContainer, container);
        _machine.WriteU32(property + _layout.PropertyId, id);
        _machine.WriteU64(property + _layout.PropertyName, nameBuffer);
        _machine.WriteU32(property + _layout.PropertyHash, StringHash::PropertyHash(typeName, name));
        _machine.WriteU32(property + _layout.PropertyOffset, offset);
        _machine.WriteU64(property + _layout.PropertyType, type);
        _machine.WriteU32(property + _layout.PropertyFlags, flags);
        return property;
    }

    void SetOptions(uint64 property, std::vector<std::pair<std::string_view, std::string_view>> const& options)
    {
        uint64 const vector = _heap.Allocate(std::max<uint64>(options.size(), 1) * _layout.OptionSize, true);
        for (std::size_t i = 0; i < options.size(); ++i)
        {
            WriteString(vector + i * _layout.OptionSize + _layout.OptionName, options[i].first);
            WriteString(vector + i * _layout.OptionSize + _layout.OptionValue, options[i].second);
        }
        _machine.WriteU64(property + _layout.PropertyOptions, vector);
        _machine.WriteU64(property + _layout.PropertyOptions + 8, vector + options.size() * _layout.OptionSize);
    }

    uint64 MapNode(uint64 key, uint64 value, bool nil)
    {
        uint64 const node = _heap.Allocate(_layout.MapNodeValue + 8, true);
        _machine.WriteU8(node + _layout.MapNodeColor, 1);
        _machine.WriteU8(node + _layout.MapNodeIsNil, nil ? 1 : 0);
        _machine.WriteU32(node + _layout.MapNodeKey, static_cast<uint32>(key));
        _machine.WriteU64(node + _layout.MapNodeValue, value);
        return node;
    }

    void Link(uint64 node, uint64 left, uint64 parent, uint64 right)
    {
        _machine.WriteU64(node + _layout.MapNodeLeft, left);
        _machine.WriteU64(node + _layout.MapNodeParent, parent);
        _machine.WriteU64(node + _layout.MapNodeRight, right);
    }

private:
    Machine& _machine;
    GuestHeap& _heap;
    ClientLayout _layout;
};

#endif
