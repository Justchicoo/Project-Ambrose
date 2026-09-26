/*
 * Project Ambrose by Imjustchico
 * The virtual tables of a client program: from the address a table starts at, each slot the relocation table lists as a pointer into code, up to the first that is not one or the next table, which code loads by its own address, with how many pointers in the program hold the same function, so an override is told from a slot the class inherits; the class the table belongs to named from its run-time type information where the program keeps it, and every table the program keeps that information for found by the class name it gives.
 */

#ifndef AMBROSE_VIRTUALTABLES_H
#define AMBROSE_VIRTUALTABLES_H

#include "Types.h"

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class CodeIndex;
class PeImage;

struct VirtualSlot
{
    uint64 Site = 0;
    uint64 Target = 0;
    std::size_t Holders = 0;
};

enum class VirtualTableEnd
{
    NotAPointer,
    NotCode,
    NextTable,
    Limit
};

struct VirtualTable
{
    uint64 Address = 0;
    std::vector<VirtualSlot> Slots;
    VirtualTableEnd End = VirtualTableEnd::NotAPointer;
    std::string ClassName;
    std::string Decorated;
    uint32 ObjectOffset = 0;
};

class VirtualTables
{
public:
    static constexpr std::size_t MaxSlots = 2048;

    VirtualTables(PeImage const& image, CodeIndex const& code);

    std::optional<VirtualTable> Read(uint64 address) const;
    std::vector<VirtualTable> FindByClass(std::string_view name) const;
    bool IsPointer(uint64 site) const;

    static std::string Undecorate(std::string_view decorated);

private:
    struct TypedTable
    {
        uint64 Address = 0;
        std::string Decorated;
        uint32 ObjectOffset = 0;
    };

    std::optional<uint64> PointerAt(uint64 site) const;
    bool IsCode(uint64 address) const;
    std::optional<TypedTable> TypeOf(uint64 locatorSite) const;
    void IndexTypes() const;

    PeImage const& _image;
    CodeIndex const& _code;
    std::vector<uint32> _pointers;
    mutable std::once_flag _typesIndexed;
    mutable std::vector<TypedTable> _typed;
};

#endif
