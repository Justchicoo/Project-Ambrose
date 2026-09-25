/*
 * Project Ambrose by Imjustchico
 * The one way a running server changes its world database: a statement from a GM command or the admin API runs on the live world database first and is written into the world edit journal only once the database has taken it, with who asked and where it came in, so the journal holds exactly the edits the database holds and an export of it replays them on another world database.
 */

#ifndef AMBROSE_WORLDEDITS_H
#define AMBROSE_WORLDEDITS_H

#include <string>

namespace WorldEdits
{
    bool Apply(std::string who, std::string source, std::string statement, std::string& error);
}

#endif
