/*
 * Project Ambrose by Imjustchico
 * Defines the global login, characters, and world database pools.
 */

#include "DatabaseEnv.h"

DatabaseWorkerPool<LoginDatabaseConnection> LoginDatabase("login");
DatabaseWorkerPool<CharacterDatabaseConnection> CharacterDatabase("characters");
DatabaseWorkerPool<WorldDatabaseConnection> WorldDatabase("world");
