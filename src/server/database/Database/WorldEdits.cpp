/*
 * Project Ambrose by Imjustchico
 * Runs a live world edit and journals it: an empty statement or a world database that is not open is refused before anything runs, a statement the database refuses is reported and never journaled, and one it takes is recorded with its author and source.
 */

#include "WorldEdits.h"
#include "DatabaseEnv.h"
#include "StringUtil.h"
#include "WorldEditJournal.h"

#include <utility>

bool WorldEdits::Apply(std::string who, std::string source, std::string statement, std::string& error)
{
    if (Ambrose::Trim(statement).empty())
    {
        error = "a world edit needs a statement to run";
        return false;
    }
    if (!WorldDatabase.IsOpen())
    {
        error = "the world database is not open, so nothing was changed";
        return false;
    }
    if (!WorldDatabase.DirectExecute(statement))
    {
        error = "the world database refused the statement, so nothing was journaled; the server log names why";
        return false;
    }
    sWorldEditJournal.Record(std::move(who), std::move(source), std::move(statement));
    return true;
}
