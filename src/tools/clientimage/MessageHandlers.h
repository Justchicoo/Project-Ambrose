/*
 * Project Ambrose by Imjustchico
 * Finds the message handlers a client program registers: each goes in under a debug name Class::MSG_Name, with the plain MSG_Name and a pointer to its function, so the names are read from the program's data sections, the code that refers to a debug name and also reads its plain name is found through the code index, and the function whose address that code loads before it is the handler.
 */

#ifndef AMBROSE_MESSAGEHANDLERS_H
#define AMBROSE_MESSAGEHANDLERS_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class CodeIndex;
class PeImage;

struct MessageHandlerRegistration
{
    std::string Owner;
    std::string Handler;
    uint64 Site = 0;
    uint64 Address = 0;
};

namespace MessageHandlers
{
    std::optional<std::size_t> SplitName(std::string_view text);
    std::vector<MessageHandlerRegistration> Find(PeImage const& image, CodeIndex const& index);
}

#endif
