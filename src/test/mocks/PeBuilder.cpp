/*
 * Project Ambrose by Imjustchico
 * Lays out PeBuilder images: user sections first, then generated unwind, exception, import, export and TLS sections, the base relocation section last, and the DOS, NT and section headers over them.
 */

#include "PeBuilder.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace
{
    uint32 AlignUp(uint32 value, uint32 alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }

    void Put16(std::vector<uint8>& out, std::size_t offset, uint16 value)
    {
        if (out.size() < offset + 2)
            out.resize(offset + 2);
        out[offset] = static_cast<uint8>(value);
        out[offset + 1] = static_cast<uint8>(value >> 8);
    }

    void Put32(std::vector<uint8>& out, std::size_t offset, uint32 value)
    {
        if (out.size() < offset + 4)
            out.resize(offset + 4);
        for (int i = 0; i < 4; ++i)
            out[offset + i] = static_cast<uint8>(value >> (8 * i));
    }

    void Put64(std::vector<uint8>& out, std::size_t offset, uint64 value)
    {
        if (out.size() < offset + 8)
            out.resize(offset + 8);
        for (int i = 0; i < 8; ++i)
            out[offset + i] = static_cast<uint8>(value >> (8 * i));
    }

    std::size_t PutString(std::vector<uint8>& out, std::string_view text)
    {
        std::size_t const offset = out.size();
        out.insert(out.end(), text.begin(), text.end());
        out.push_back(0);
        return offset;
    }

    std::string SlotKey(std::string_view dll, std::string_view name)
    {
        std::string key(dll);
        key += '!';
        key += name;
        return key;
    }
}

PeBuilder::PeBuilder(uint64 imageBase, bool dll) : _imageBase(imageBase), _dll(dll)
{
}

uint32 PeBuilder::NextSectionRva() const
{
    if (_sections.empty())
        return SectionAlignment;
    Section const& last = _sections.back();
    uint32 const size = std::max<uint32>(last.VirtualSize, static_cast<uint32>(last.Data.size()));
    return AlignUp(last.Rva + std::max<uint32>(size, 1), SectionAlignment);
}

uint32 PeBuilder::Append(std::string name, std::vector<uint8> data, uint32 characteristics, uint32 virtualSize)
{
    Section section;
    section.Name = std::move(name);
    section.Rva = NextSectionRva();
    section.VirtualSize = std::max<uint32>(virtualSize, static_cast<uint32>(data.size()));
    section.Data = std::move(data);
    section.Characteristics = characteristics;
    _sections.push_back(std::move(section));
    return _sections.back().Rva;
}

uint32 PeBuilder::AddSection(std::string name, std::vector<uint8> data, uint32 characteristics, uint32 virtualSize)
{
    return Append(std::move(name), std::move(data), characteristics, virtualSize);
}

void PeBuilder::SetEntryPoint(uint32 rva)
{
    _entryPoint = rva;
}

void PeBuilder::SetTimeDateStamp(uint32 stamp)
{
    _timeDateStamp = stamp;
}

void PeBuilder::SetExportName(std::string dllName)
{
    _exportName = std::move(dllName);
}

void PeBuilder::AddExport(std::string name, uint32 rva)
{
    _exports.push_back({ std::move(name), rva, {} });
}

void PeBuilder::AddForwarder(std::string name, std::string target)
{
    _exports.push_back({ std::move(name), 0, std::move(target) });
}

void PeBuilder::AddImport(std::string dll, std::string name)
{
    auto found = std::find_if(_imports.begin(), _imports.end(), [&](auto const& entry) { return entry.first == dll; });
    if (found == _imports.end())
    {
        _imports.emplace_back(dll, std::vector<Import>{});
        found = std::prev(_imports.end());
    }
    found->second.push_back({ std::move(name), std::nullopt });
}

void PeBuilder::AddImportByOrdinal(std::string dll, uint16 ordinal)
{
    auto found = std::find_if(_imports.begin(), _imports.end(), [&](auto const& entry) { return entry.first == dll; });
    if (found == _imports.end())
    {
        _imports.emplace_back(dll, std::vector<Import>{});
        found = std::prev(_imports.end());
    }
    found->second.push_back({ {}, ordinal });
}

void PeBuilder::AddRelocation(uint32 rva)
{
    _relocations.push_back(rva);
}

void PeBuilder::SetTls(uint32 templateRva, uint32 templateSize, uint32 indexRva, std::vector<uint32> callbackRvas, uint32 zeroFill)
{
    _tls = Tls{ templateRva, templateSize, indexRva, std::move(callbackRvas), zeroFill };
}

void PeBuilder::AddFunction(uint32 begin, uint32 end)
{
    _functions.push_back({ begin, end, std::nullopt });
}

void PeBuilder::AddChainedFunction(uint32 begin, uint32 end, uint32 primaryBegin, uint32 primaryEnd)
{
    _functions.push_back({ begin, end, std::pair<uint32, uint32>{ primaryBegin, primaryEnd } });
}

uint32 PeBuilder::ImportSlotRva(std::string_view dll, std::string_view name) const
{
    auto const found = _slots.find(SlotKey(dll, name));
    if (found == _slots.end())
        throw std::out_of_range("no such import slot");
    return found->second;
}

uint32 PeBuilder::ImportSlotRva(std::string_view dll, uint16 ordinal) const
{
    return ImportSlotRva(dll, "#" + std::to_string(ordinal));
}

std::vector<uint8> PeBuilder::Build()
{
    uint32 exceptionRva = 0;
    uint32 exceptionSize = 0;
    if (!_functions.empty())
    {
        std::vector<Function> functions = _functions;
        std::sort(functions.begin(), functions.end(), [](Function const& a, Function const& b) { return a.Begin < b.Begin; });
        std::vector<uint8> xdata;
        std::map<uint32, uint32> primaryUnwind;
        uint32 const xdataRva = NextSectionRva();
        for (Function const& function : functions)
        {
            if (function.Primary)
                continue;
            primaryUnwind[function.Begin] = xdataRva + static_cast<uint32>(xdata.size());
            xdata.insert(xdata.end(), { 0x01, 0x00, 0x00, 0x00 });
        }
        std::map<uint32, uint32> chainedUnwind;
        for (Function const& function : functions)
        {
            if (!function.Primary)
                continue;
            if (!primaryUnwind.contains(function.Primary->first))
            {
                primaryUnwind[function.Primary->first] = xdataRva + static_cast<uint32>(xdata.size());
                xdata.insert(xdata.end(), { 0x01, 0x00, 0x00, 0x00 });
            }
            chainedUnwind[function.Begin] = xdataRva + static_cast<uint32>(xdata.size());
            std::size_t const at = xdata.size();
            xdata.insert(xdata.end(), { 0x01 | (0x04 << 3), 0x00, 0x00, 0x00 });
            Put32(xdata, at + 4, function.Primary->first);
            Put32(xdata, at + 8, function.Primary->second);
            Put32(xdata, at + 12, primaryUnwind[function.Primary->first]);
        }
        Append(".xdata", std::move(xdata), ReadOnlyCharacteristics, 0);
        std::vector<uint8> pdata;
        for (Function const& function : functions)
        {
            std::size_t const at = pdata.size();
            Put32(pdata, at, function.Begin);
            Put32(pdata, at + 4, function.End);
            Put32(pdata, at + 8, function.Primary ? chainedUnwind[function.Begin] : primaryUnwind[function.Begin]);
        }
        exceptionSize = static_cast<uint32>(pdata.size());
        exceptionRva = Append(".pdata", std::move(pdata), ReadOnlyCharacteristics, 0);
    }

    uint32 importRva = 0;
    uint32 importSize = 0;
    uint32 iatRva = 0;
    uint32 iatSize = 0;
    if (!_imports.empty())
    {
        uint32 const base = NextSectionRva();
        std::vector<uint8> idata;
        std::size_t const descriptorsSize = (_imports.size() + 1) * 20;
        idata.resize(descriptorsSize);
        std::vector<std::size_t> iltOffsets;
        std::vector<std::size_t> iatOffsets;
        for (auto const& [dll, imports] : _imports)
        {
            iltOffsets.push_back(idata.size());
            idata.resize(idata.size() + (imports.size() + 1) * 8);
        }
        std::size_t const iatStart = idata.size();
        for (auto const& [dll, imports] : _imports)
        {
            iatOffsets.push_back(idata.size());
            idata.resize(idata.size() + (imports.size() + 1) * 8);
        }
        iatRva = base + static_cast<uint32>(iatStart);
        iatSize = static_cast<uint32>(idata.size() - iatStart);
        for (std::size_t d = 0; d < _imports.size(); ++d)
        {
            auto const& [dll, imports] = _imports[d];
            std::size_t const nameOffset = PutString(idata, dll);
            Put32(idata, d * 20, base + static_cast<uint32>(iltOffsets[d]));
            Put32(idata, d * 20 + 12, base + static_cast<uint32>(nameOffset));
            Put32(idata, d * 20 + 16, base + static_cast<uint32>(iatOffsets[d]));
            for (std::size_t i = 0; i < imports.size(); ++i)
            {
                uint64 thunk = 0;
                if (imports[i].Ordinal)
                {
                    thunk = 0x8000000000000000ull | *imports[i].Ordinal;
                    _slots[SlotKey(dll, "#" + std::to_string(*imports[i].Ordinal))] = base + static_cast<uint32>(iatOffsets[d] + i * 8);
                }
                else
                {
                    if (idata.size() % 2)
                        idata.push_back(0);
                    std::size_t const hint = idata.size();
                    idata.push_back(0);
                    idata.push_back(0);
                    PutString(idata, imports[i].Name);
                    thunk = base + static_cast<uint32>(hint);
                    _slots[SlotKey(dll, imports[i].Name)] = base + static_cast<uint32>(iatOffsets[d] + i * 8);
                }
                Put64(idata, iltOffsets[d] + i * 8, thunk);
                Put64(idata, iatOffsets[d] + i * 8, thunk);
            }
        }
        importSize = static_cast<uint32>(descriptorsSize);
        importRva = Append(".idata", std::move(idata), DataCharacteristics, 0);
    }

    uint32 exportRva = 0;
    uint32 exportSize = 0;
    if (!_exports.empty())
    {
        std::vector<Export> exports = _exports;
        std::sort(exports.begin(), exports.end(), [](Export const& a, Export const& b) { return a.Name < b.Name; });
        uint32 const base = NextSectionRva();
        std::vector<uint8> edata(40);
        std::size_t const functionsOffset = edata.size();
        edata.resize(edata.size() + exports.size() * 4);
        std::size_t const namesOffset = edata.size();
        edata.resize(edata.size() + exports.size() * 4);
        std::size_t const ordinalsOffset = edata.size();
        edata.resize(edata.size() + exports.size() * 2);
        std::size_t const dllName = PutString(edata, _exportName);
        for (std::size_t i = 0; i < exports.size(); ++i)
        {
            std::size_t const name = PutString(edata, exports[i].Name);
            Put32(edata, namesOffset + i * 4, base + static_cast<uint32>(name));
            Put16(edata, ordinalsOffset + i * 2, static_cast<uint16>(i));
            if (exports[i].Forwarder.empty())
                Put32(edata, functionsOffset + i * 4, exports[i].Rva);
        }
        for (std::size_t i = 0; i < exports.size(); ++i)
        {
            if (exports[i].Forwarder.empty())
                continue;
            std::size_t const forwarder = PutString(edata, exports[i].Forwarder);
            Put32(edata, functionsOffset + i * 4, base + static_cast<uint32>(forwarder));
        }
        Put32(edata, 4, _timeDateStamp);
        Put32(edata, 12, base + static_cast<uint32>(dllName));
        Put32(edata, 16, 1);
        Put32(edata, 20, static_cast<uint32>(exports.size()));
        Put32(edata, 24, static_cast<uint32>(exports.size()));
        Put32(edata, 28, base + static_cast<uint32>(functionsOffset));
        Put32(edata, 32, base + static_cast<uint32>(namesOffset));
        Put32(edata, 36, base + static_cast<uint32>(ordinalsOffset));
        exportSize = static_cast<uint32>(edata.size());
        exportRva = Append(".edata", std::move(edata), ReadOnlyCharacteristics, 0);
    }

    uint32 tlsRva = 0;
    std::vector<uint32> relocations = _relocations;
    if (_tls)
    {
        uint32 const base = NextSectionRva();
        std::vector<uint8> tls(40);
        Put64(tls, 0, _imageBase + _tls->TemplateRva);
        Put64(tls, 8, _imageBase + _tls->TemplateRva + _tls->TemplateSize);
        Put64(tls, 16, _imageBase + _tls->IndexRva);
        Put32(tls, 32, _tls->ZeroFill);
        for (uint32 field = 0; field < 3; ++field)
            relocations.push_back(base + field * 8);
        if (!_tls->Callbacks.empty())
        {
            std::size_t const array = tls.size();
            Put64(tls, 24, _imageBase + base + static_cast<uint32>(array));
            relocations.push_back(base + 24);
            for (std::size_t i = 0; i < _tls->Callbacks.size(); ++i)
            {
                Put64(tls, array + i * 8, _imageBase + _tls->Callbacks[i]);
                relocations.push_back(base + static_cast<uint32>(array + i * 8));
            }
            Put64(tls, array + _tls->Callbacks.size() * 8, 0);
        }
        tlsRva = Append(".tls", std::move(tls), DataCharacteristics, 0);
    }

    uint32 relocRva = 0;
    uint32 relocSize = 0;
    if (!relocations.empty())
    {
        std::sort(relocations.begin(), relocations.end());
        std::vector<uint8> reloc;
        std::size_t i = 0;
        while (i < relocations.size())
        {
            uint32 const page = relocations[i] & ~0xFFFu;
            std::size_t const blockStart = reloc.size();
            reloc.resize(reloc.size() + 8);
            while (i < relocations.size() && (relocations[i] & ~0xFFFu) == page)
            {
                std::size_t const at = reloc.size();
                Put16(reloc, at, static_cast<uint16>((10 << 12) | (relocations[i] & 0xFFF)));
                ++i;
            }
            if ((reloc.size() - blockStart) % 4)
                Put16(reloc, reloc.size(), 0);
            Put32(reloc, blockStart, page);
            Put32(reloc, blockStart + 4, static_cast<uint32>(reloc.size() - blockStart));
        }
        relocSize = static_cast<uint32>(reloc.size());
        relocRva = Append(".reloc", std::move(reloc), ReadOnlyCharacteristics, 0);
    }

    uint32 const sizeOfImage = NextSectionRva();
    std::vector<uint8> image(HeadersSize, 0);
    image[0] = 'M';
    image[1] = 'Z';
    Put32(image, 0x3C, 0x80);
    std::size_t const nt = 0x80;
    std::memcpy(image.data() + nt, "PE\0\0", 4);
    std::size_t const file = nt + 4;
    Put16(image, file, 0x8664);
    Put16(image, file + 2, static_cast<uint16>(_sections.size()));
    Put32(image, file + 4, _timeDateStamp);
    Put16(image, file + 16, 0xF0);
    Put16(image, file + 18, static_cast<uint16>(_dll ? 0x2022 : 0x0022));
    std::size_t const optional = file + 20;
    Put16(image, optional, 0x20B);
    Put32(image, optional + 16, _entryPoint);
    Put64(image, optional + 24, _imageBase);
    Put32(image, optional + 32, SectionAlignment);
    Put32(image, optional + 36, FileAlignment);
    Put16(image, optional + 40, 6);
    Put16(image, optional + 48, 6);
    Put32(image, optional + 56, sizeOfImage);
    Put32(image, optional + 60, HeadersSize);
    Put16(image, optional + 68, 3);
    Put16(image, optional + 70, 0x8160);
    Put64(image, optional + 72, 0x100000);
    Put64(image, optional + 80, 0x1000);
    Put64(image, optional + 88, 0x100000);
    Put64(image, optional + 96, 0x1000);
    Put32(image, optional + 108, 16);
    std::size_t const directories = optional + 112;
    auto directory = [&](int index, uint32 rva, uint32 size)
    {
        Put32(image, directories + index * 8, rva);
        Put32(image, directories + index * 8 + 4, size);
    };
    directory(0, exportRva, exportSize);
    directory(1, importRva, importSize);
    directory(3, exceptionRva, exceptionSize);
    directory(5, relocRva, relocSize);
    directory(9, tlsRva, tlsRva ? 40 : 0);
    directory(12, iatRva, iatSize);

    std::size_t const headers = directories + 16 * 8;
    uint32 fileOffset = HeadersSize;
    for (std::size_t s = 0; s < _sections.size(); ++s)
    {
        Section const& section = _sections[s];
        std::size_t const at = headers + s * 40;
        std::memcpy(image.data() + at, section.Name.data(), std::min<std::size_t>(section.Name.size(), 8));
        uint32 const rawSize = AlignUp(static_cast<uint32>(section.Data.size()), FileAlignment);
        Put32(image, at + 8, section.VirtualSize);
        Put32(image, at + 12, section.Rva);
        Put32(image, at + 16, rawSize);
        Put32(image, at + 20, rawSize ? fileOffset : 0);
        Put32(image, at + 36, section.Characteristics);
        fileOffset += rawSize;
    }
    if (headers + _sections.size() * 40 > HeadersSize)
        throw std::length_error("too many sections for the header area");
    for (Section const& section : _sections)
    {
        std::vector<uint8> raw = section.Data;
        raw.resize(AlignUp(static_cast<uint32>(raw.size()), FileAlignment));
        image.insert(image.end(), raw.begin(), raw.end());
    }
    return image;
}
