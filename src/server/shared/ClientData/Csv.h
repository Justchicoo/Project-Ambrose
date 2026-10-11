/*
 * Project Ambrose by Imjustchico
 * Quotes a value as one CSV field, doubling every embedded double quote.
 */

#ifndef AMBROSE_CSV_H
#define AMBROSE_CSV_H

#include <string>
#include <string_view>

namespace Csv
{
    inline std::string Quote(std::string_view text)
    {
        std::string out = "\"";
        for (char c : text)
        {
            if (c == '"')
                out += '"';
            out += c;
        }
        return out + "\"";
    }
}

#endif
