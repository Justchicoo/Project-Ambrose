/*
 * Project Ambrose by Imjustchico
 * Renders a source file the one way every consumer reads it, with the separators a link and a repository both expect, since __FILE__ keeps whichever the compiler was handed.
 */

#include "LogSource.h"

#include <algorithm>

namespace LogSourcePath
{
std::string Portable(std::string_view file)
{
    std::string out(file);
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}
}
