/*
 * Project Ambrose by Imjustchico
 * What the bits of a value no class names could be, read the way the client's data files write values: one bit as a bool, 32 as an int, an unsigned int or a float, 64 as a 64-bit integer or a double, 96 as three floats, and whole bytes as a text of compact length or a list of such texts when they read to the last bit, so a value can be named by what it holds.
 */

#ifndef AMBROSE_SKIPPEDVALUE_H
#define AMBROSE_SKIPPEDVALUE_H

#include "Types.h"

#include <span>
#include <string>
#include <vector>

namespace SkippedValue
{
    std::vector<std::string> Readings(uint64 bits, std::span<uint8 const> value);
    std::string Describe(uint64 bits, std::span<uint8 const> value);
}

#endif
