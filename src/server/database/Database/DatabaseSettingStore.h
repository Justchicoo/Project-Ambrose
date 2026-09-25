/*
 * Project Ambrose by Imjustchico
 * The settings store an app hands the live settings registry, over the database it owns: the game server's is the characters database and the login and patch servers' the login database, both holding the same settings and setting_audit tables.
 */

#ifndef AMBROSE_DATABASESETTINGSTORE_H
#define AMBROSE_DATABASESETTINGSTORE_H

#include "Settings.h"

#include <memory>

namespace SettingStores
{
    std::shared_ptr<SettingStore> ForCharacters();
    std::shared_ptr<SettingStore> ForLogin();
}

#endif
