/*
 * Project Ambrose by Imjustchico
 * Lays out a KIWAD header, entry records with NUL-terminated names, and entry data in file order.
 */

#include "KiwadBuilder.h"
#include "Compression.h"
#include "Crc32.h"

namespace
{
    void AppendUInt32(std::vector<uint8>& out, uint32 value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            out.push_back(static_cast<uint8>(value >> shift));
    }
}

KiwadBuilder::KiwadBuilder(uint32 version, uint8 flags) : _version(version), _flags(flags)
{
}

KiwadBuilder& KiwadBuilder::Add(std::string name, std::vector<uint8> data, bool compress)
{
    _items.push_back({ std::move(name), std::move(data), compress });
    return *this;
}

KiwadBuilder& KiwadBuilder::Add(std::string name, std::string_view text, bool compress)
{
    return Add(std::move(name), std::vector<uint8>(text.begin(), text.end()), compress);
}

std::size_t KiwadBuilder::GetTocLength() const
{
    std::size_t length = _version >= 2 ? 14 : 13;
    for (Item const& item : _items)
        length += 21 + item.Name.size() + 1;
    return length;
}

std::vector<uint8> KiwadBuilder::Build() const
{
    std::vector<std::vector<uint8>> stored;
    for (Item const& item : _items)
        stored.push_back(item.Compress ? Ambrose::Compression::Deflate(item.Data) : item.Data);

    std::vector<uint8> out{ 'K', 'I', 'W', 'A', 'D' };
    AppendUInt32(out, _version);
    AppendUInt32(out, static_cast<uint32>(_items.size()));
    if (_version >= 2)
        out.push_back(_flags);
    uint32 offset = static_cast<uint32>(GetTocLength());
    for (std::size_t i = 0; i < _items.size(); ++i)
    {
        Item const& item = _items[i];
        AppendUInt32(out, offset);
        AppendUInt32(out, static_cast<uint32>(item.Data.size()));
        AppendUInt32(out, item.Compress ? static_cast<uint32>(stored[i].size()) : 0xFFFFFFFFu);
        out.push_back(item.Compress ? 1 : 0);
        AppendUInt32(out, Crc32::ComputeClient(stored[i]));
        AppendUInt32(out, static_cast<uint32>(item.Name.size() + 1));
        out.insert(out.end(), item.Name.begin(), item.Name.end());
        out.push_back(0);
        offset += static_cast<uint32>(stored[i].size());
    }
    for (std::vector<uint8> const& data : stored)
        out.insert(out.end(), data.begin(), data.end());
    return out;
}
