/*
 * Project Ambrose by Imjustchico
 * The supervisor's panel user console commands: listing, making, disabling and enabling operators, resetting their password or two-factor sign-in, and handing out the local and pairing links a desktop program signs in with, those that hand out a way to sign in marked to run only on the supervisor's own console.
 */

#ifndef AMBROSE_PANELCOMMANDS_H
#define AMBROSE_PANELCOMMANDS_H

#include "Types.h"

#include <string>

class ConsoleCommandTable;
class Panel;

class PanelCommands
{
public:
    PanelCommands() = delete;

    static void Register(ConsoleCommandTable& commands, Panel& panel);
    static std::string WhenText(int64 epochMs);
};

#endif
