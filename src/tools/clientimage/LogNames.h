/*
 * Project Ambrose by Imjustchico
 * The names the client program's functions give themselves in their log lines. The client's log macro loads the source file's path and the function's own qualified name, such as CoreObjectFactory::AddBehavior, one right after the other, so a function that reads a qualified name within NeighborBytes of reading a source path logs under that name; a function that a logging function was inlined into logs under that one's name as well, so every name a function logs under is kept, in the order its code reads them.
 */

#ifndef AMBROSE_LOGNAMES_H
#define AMBROSE_LOGNAMES_H

#include "Types.h"

#include <string>
#include <string_view>
#include <vector>

class CodeIndex;
class PeImage;

struct FunctionLogNames
{
    uint64 Function = 0;
    std::vector<std::string> Names;
};

namespace LogNames
{
    inline constexpr uint64 NeighborBytes = 32;

    bool IsQualifiedName(std::string_view text);
    bool IsSourcePath(std::string_view text);
    std::vector<FunctionLogNames> Find(PeImage const& image, CodeIndex const& code);
}

#endif
