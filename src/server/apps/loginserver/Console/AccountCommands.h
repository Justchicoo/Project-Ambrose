/*
 * Project Ambrose by Imjustchico
 * The login server's account console commands, available until the command framework of milestone 4.02 takes them over.
 */

#ifndef AMBROSE_ACCOUNTCOMMANDS_H
#define AMBROSE_ACCOUNTCOMMANDS_H

#include "ConsoleCommandTable.h"

namespace AccountCommands
{
    void Register(ConsoleCommandTable& commands);
    void Unregister(ConsoleCommandTable& commands);
}

#endif
