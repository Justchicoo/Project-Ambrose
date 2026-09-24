/*
 * Project Ambrose by Imjustchico
 * The three global database pools every server shares: login, characters, and world.
 */

#ifndef AMBROSE_DATABASEENV_H
#define AMBROSE_DATABASEENV_H

#include "CharacterDatabase.h"
#include "DatabaseWorkerPool.h"
#include "LoginDatabase.h"
#include "QueryHolder.h"
#include "QueryResult.h"
#include "WorldDatabase.h"

extern DatabaseWorkerPool<LoginDatabaseConnection> LoginDatabase;
extern DatabaseWorkerPool<CharacterDatabaseConnection> CharacterDatabase;
extern DatabaseWorkerPool<WorldDatabaseConnection> WorldDatabase;

#endif
