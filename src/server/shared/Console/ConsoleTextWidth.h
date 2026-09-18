/*
 * Project Ambrose by Imjustchico
 * The width a UTF-8 string takes in terminal columns, and the character boundaries a console prompt walks it by.
 */

#ifndef AMBROSE_CONSOLETEXTWIDTH_H
#define AMBROSE_CONSOLETEXTWIDTH_H

#include <cstddef>
#include <string_view>

namespace ConsoleTextWidth
{
    std::size_t Columns(std::string_view text);
    std::size_t CharacterColumns(std::string_view character);
    std::size_t Next(std::string_view text, std::size_t index);
}

#endif
