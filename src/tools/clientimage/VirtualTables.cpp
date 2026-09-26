/*
 * Project Ambrose by Imjustchico
 * Reads a table slot by slot through the relocation table, so only what the loader would fix up as a pointer counts, and names its class from the complete object locator the slot before it points to, a locator that must point back at itself and at a type descriptor whose decorated name the program holds; the locators are found once, from every pointer the relocation table lists.
 */

#include "VirtualTables.h"
#include "CodeIndex.h"
#include "PeImage.h"
#include "StringUtil.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>

namespace
{
    constexpr uint32 LocatorSignature = 1;
    constexpr uint32 LocatorSize = 24;
    constexpr uint32 TypeNameOffset = 16;
    constexpr std::size_t MaxTypeName = 4096;

    uint32 Get32(uint8 const* data)
    {
        return static_cast<uint32>(data[0]) | static_cast<uint32>(data[1]) << 8 | static_cast<uint32>(data[2]) << 16 | static_cast<uint32>(data[3]) << 24;
    }

    std::string_view WithoutKeyword(std::string_view name)
    {
        for (std::string_view const keyword : { std::string_view("class "), std::string_view("struct ") })
            if (name.starts_with(keyword))
                return name.substr(keyword.size());
        return name;
    }
}

VirtualTables::VirtualTables(PeImage const& image, CodeIndex const& code) : _image(image), _code(code)
{
    for (PeRelocation const& relocation : image.GetRelocations())
        if (relocation.Type == PeImage::RelocationDir64)
            _pointers.push_back(relocation.Rva);
    std::sort(_pointers.begin(), _pointers.end());
    _pointers.erase(std::unique(_pointers.begin(), _pointers.end()), _pointers.end());
}

bool VirtualTables::IsPointer(uint64 site) const
{
    uint64 const base = _image.GetImageBase();
    if (site < base || site - base > std::numeric_limits<uint32>::max())
        return false;
    return std::binary_search(_pointers.begin(), _pointers.end(), static_cast<uint32>(site - base));
}

std::optional<uint64> VirtualTables::PointerAt(uint64 site) const
{
    if (!IsPointer(site))
        return std::nullopt;
    std::span<uint8 const> const bytes = _image.ReadRva(static_cast<uint32>(site - _image.GetImageBase()), 8);
    if (bytes.size() != 8)
        return std::nullopt;
    uint64 value = 0;
    for (std::size_t index = 0; index < 8; ++index)
        value |= uint64{ bytes[index] } << (8 * index);
    return value;
}

bool VirtualTables::IsCode(uint64 address) const
{
    uint64 const base = _image.GetImageBase();
    if (address < base || address - base >= _image.GetSizeOfImage())
        return false;
    PeSection const* const section = _image.SectionOfRva(static_cast<uint32>(address - base));
    return section != nullptr && section->IsExecutable();
}

std::optional<VirtualTables::TypedTable> VirtualTables::TypeOf(uint64 locatorSite) const
{
    std::optional<uint64> const locator = PointerAt(locatorSite);
    uint64 const base = _image.GetImageBase();
    if (!locator || *locator < base || IsCode(*locator) || *locator - base >= _image.GetSizeOfImage())
        return std::nullopt;
    uint32 const locatorRva = static_cast<uint32>(*locator - base);
    std::span<uint8 const> const fields = _image.ReadRva(locatorRva, LocatorSize);
    if (fields.size() != LocatorSize || Get32(fields.data()) != LocatorSignature || Get32(fields.data() + 20) != locatorRva)
        return std::nullopt;
    uint32 const descriptor = Get32(fields.data() + 12);
    PeSection const* const section = _image.SectionOfRva(descriptor);
    if (section == nullptr || section->IsExecutable())
        return std::nullopt;
    uint32 const available = section->VirtualAddress + std::max(section->VirtualSize, section->RawSize) - descriptor;
    if (available <= TypeNameOffset)
        return std::nullopt;
    std::span<uint8 const> const text = _image.ReadRva(descriptor + TypeNameOffset, static_cast<uint32>(std::min<std::size_t>(available - TypeNameOffset, MaxTypeName)));
    void const* const terminator = text.empty() ? nullptr : std::memchr(text.data(), 0, text.size());
    if (terminator == nullptr)
        return std::nullopt;
    std::string name(reinterpret_cast<char const*>(text.data()), static_cast<std::size_t>(static_cast<uint8 const*>(terminator) - text.data()));
    if (!name.starts_with(".?A"))
        return std::nullopt;
    return TypedTable{ locatorSite + 8, std::move(name), Get32(fields.data() + 4) };
}

std::optional<VirtualTable> VirtualTables::Read(uint64 address) const
{
    VirtualTable table;
    table.Address = address;
    for (std::size_t index = 0;; ++index)
    {
        if (index == MaxSlots)
        {
            table.End = VirtualTableEnd::Limit;
            break;
        }
        uint64 const site = address + index * 8;
        if (index > 0 && !_code.LeaReferences(site).empty())
        {
            table.End = VirtualTableEnd::NextTable;
            break;
        }
        std::optional<uint64> const target = PointerAt(site);
        if (!target)
        {
            table.End = VirtualTableEnd::NotAPointer;
            break;
        }
        if (!IsCode(*target))
        {
            table.End = VirtualTableEnd::NotCode;
            break;
        }
        table.Slots.push_back({ site, *target, _code.PointerSites(*target).size() });
    }
    if (table.Slots.empty())
        return std::nullopt;
    if (std::optional<TypedTable> typed = TypeOf(address - 8))
    {
        table.ClassName = Undecorate(typed->Decorated);
        table.Decorated = std::move(typed->Decorated);
        table.ObjectOffset = typed->ObjectOffset;
    }
    return table;
}

void VirtualTables::IndexTypes() const
{
    uint64 const base = _image.GetImageBase();
    for (uint32 const rva : _pointers)
    {
        uint64 const site = base + rva;
        if (!IsPointer(site + 8))
            continue;
        std::optional<uint64> const first = PointerAt(site + 8);
        if (!first || !IsCode(*first))
            continue;
        if (std::optional<TypedTable> typed = TypeOf(site))
            _typed.push_back(std::move(*typed));
    }
}

std::vector<VirtualTable> VirtualTables::FindByClass(std::string_view name) const
{
    std::call_once(_typesIndexed, [this] { IndexTypes(); });
    std::string const wanted = Ambrose::ToLower(WithoutKeyword(name));
    std::vector<VirtualTable> tables;
    for (TypedTable const& typed : _typed)
    {
        if (Ambrose::ToLower(Undecorate(typed.Decorated)) != wanted && Ambrose::ToLower(typed.Decorated) != wanted)
            continue;
        if (std::optional<VirtualTable> table = Read(typed.Address))
            tables.push_back(std::move(*table));
    }
    std::sort(tables.begin(), tables.end(), [](VirtualTable const& left, VirtualTable const& right) { return left.ObjectOffset < right.ObjectOffset; });
    return tables;
}

std::string VirtualTables::Undecorate(std::string_view decorated)
{
    if ((!decorated.starts_with(".?AV") && !decorated.starts_with(".?AU")) || !decorated.ends_with("@@") || decorated.size() <= 6)
        return std::string(decorated);
    std::string_view const qualified = decorated.substr(4, decorated.size() - 6);
    if (qualified.find_first_of("?$@") == std::string_view::npos)
        return std::string(qualified);
    if (qualified.find_first_of("?$") != std::string_view::npos || qualified.find("@@") != std::string_view::npos || qualified.front() == '@' || qualified.back() == '@')
        return std::string(decorated);
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= qualified.size())
    {
        std::size_t const at = qualified.find('@', start);
        std::size_t const end = at == std::string_view::npos ? qualified.size() : at;
        parts.push_back(qualified.substr(start, end - start));
        start = end + 1;
    }
    std::string name;
    for (auto part = parts.rbegin(); part != parts.rend(); ++part)
    {
        if (!name.empty())
            name += "::";
        name += *part;
    }
    return name;
}
