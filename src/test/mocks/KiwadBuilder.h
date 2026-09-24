/*
 * Project Ambrose by Imjustchico
 * Builds synthetic KIWAD archives in memory with stored or compressed entries and correct client CRCs, for archive tests.
 */

#ifndef AMBROSE_KIWADBUILDER_H
#define AMBROSE_KIWADBUILDER_H

#include "Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class KiwadBuilder
{
public:
    explicit KiwadBuilder(uint32 version = 1, uint8 flags = 0);

    KiwadBuilder& Add(std::string name, std::vector<uint8> data, bool compress);
    KiwadBuilder& Add(std::string name, std::string_view text, bool compress);
    std::vector<uint8> Build() const;
    std::size_t GetTocLength() const;

private:
    struct Item
    {
        std::string Name;
        std::vector<uint8> Data;
        bool Compress;
    };

    uint32 _version;
    uint8 _flags;
    std::vector<Item> _items;
};

#endif
