/*
 * Project Ambrose by Imjustchico
 * The DB log appender (type 4): hands every accepted line to the database log sink for batched inserts into the login database's logs table, never for sql categories, plus helpers that enable and disable it around an app's database lifetime.
 */

#ifndef AMBROSE_APPENDERDB_H
#define AMBROSE_APPENDERDB_H

#include "AppenderRegistry.h"

#include <atomic>

class Log;

class AppenderDB : public Appender
{
public:
    AppenderDB(AppenderDefinition const& definition, uint32 realmId);

    static AppenderTypeInfo GetTypeInfo();
    static bool Enable(Log& log, uint32 defaultRealmId);
    static void Disable(Log& log);

    void Flush() override;

protected:
    void WriteMessage(LogMessage const& message) override;

private:
    uint32 _realmId;
    static inline std::atomic<uint32> _defaultRealmId{ 0 };
};

#endif
