/*
 * Project Ambrose by Imjustchico
 * The real way the panel program asks this computer's supervisor: its admin API on loopback, plain HTTP as the supervisor serves it there, with its token and a short timeout.
 */

#ifndef AMBROSE_LOOPBACKASKER_H
#define AMBROSE_LOOPBACKASKER_H

#include "ThisComputer.h"

#include <chrono>

class LoopbackAsker : public AdminAsker
{
public:
    static constexpr std::chrono::seconds Timeout{ 5 };

    AdminAnswer Ask(uint16 port, std::string const& token, std::string_view method, std::string_view path, std::string const& body) override;
};

#endif
