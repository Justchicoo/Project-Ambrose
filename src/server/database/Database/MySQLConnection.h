/*
 * Project Ambrose by Imjustchico
 * One connection to a MariaDB or MySQL server: parses the connection string, opens and closes, prepares registered statements, runs text and prepared queries, reports the rows the last prepared statement changed, escapes, reports whether a statement is running on it, and reconnects with backoff when the server goes away.
 */

#ifndef AMBROSE_MYSQLCONNECTION_H
#define AMBROSE_MYSQLCONNECTION_H

#include "DatabaseEnvFwd.h"
#include "Types.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct st_mysql;
struct st_mysql_res;
class MySQLPreparedStatement;

enum class ConnectionFlags : uint8
{
    Async = 1,
    Sync = 2,
    Both = 3
};

enum class DatabaseTls : uint8
{
    Off,
    Required,
    RequiredVerified
};

struct MySQLConnectionInfo
{
    std::string Host;
    uint16 Port = 3306;
    std::string Socket;
    std::string User;
    std::string Password;
    std::string Database;
    DatabaseTls Tls = DatabaseTls::Off;
    std::string TlsCa;

    static std::optional<MySQLConnectionInfo> Parse(std::string_view text, std::string* error = nullptr);
    std::string ToLogString() const;
    std::string ToConnectionString() const;

    bool operator==(MySQLConnectionInfo const&) const = default;
};

struct MySQLConnectionSettings
{
    std::chrono::seconds ConnectTimeout{ 10 };
    std::chrono::seconds ReadTimeout{ 300 };
    std::chrono::seconds WriteTimeout{ 60 };
    std::chrono::milliseconds FirstReconnectDelay{ 100 };
    std::chrono::milliseconds MaxReconnectDelay{ 5000 };
    std::chrono::milliseconds GiveUpReconnectAfter{ 30000 };
    std::chrono::milliseconds ReconnectCooldown{ 10000 };
    ConnectionFlags Flags = ConnectionFlags::Both;
    bool MultiStatements = false;
};

struct TransactionResult
{
    uint32 Code = 0;
    bool CommitSent = false;
    bool RolledBack = true;
};

class TransactionBase;

struct PreparedStatementInfo
{
    uint32 Index = 0;
    std::string Name;
    std::size_t ParameterCount = 0;
};

class MySQLConnection
{
public:
    explicit MySQLConnection(MySQLConnectionInfo info, MySQLConnectionSettings settings = {});
    virtual ~MySQLConnection();

    MySQLConnection(MySQLConnection const&) = delete;
    MySQLConnection& operator=(MySQLConnection const&) = delete;

    uint32 Open();
    void Close();
    bool IsOpen() const noexcept { return _mysql != nullptr; }

    bool Execute(std::string_view sql);
    bool ExecuteScript(std::string_view sql, std::size_t& failedStatement);
    QueryResult Query(std::string_view sql);
    std::string Escape(std::string_view text);
    bool Ping();

    bool PrepareStatements();
    std::unique_ptr<PreparedStatementBase> GetPreparedStatement(uint32 index) const;
    bool Execute(PreparedStatementBase const& statement);
    PreparedQueryResult Query(PreparedStatementBase const& statement);
    bool IsStatementPrepared(uint32 index) const noexcept;
    TransactionResult ExecuteTransaction(TransactionBase const& transaction);
    std::size_t GetPreparedStatementCount() const noexcept;
    std::vector<PreparedStatementInfo> GetPreparedStatementInfos() const;
    uint64 GetConcurrentUseCount() const noexcept { return _concurrentUses.load(std::memory_order_relaxed); }
    bool IsInUse() const noexcept { return _activeUsers.load(std::memory_order_relaxed) > 0; }

    uint32 GetLastErrorCode() const noexcept { return _lastErrorCode; }
    uint64 GetLastAffectedRows() const noexcept { return _lastAffectedRows; }
    std::string const& GetLastErrorText() const noexcept { return _lastErrorText; }
    std::string GetServerInfo() const;
    uint64 GetServerVersion() const;
    uint64 GetThreadId() const;
    bool IsMariaDB() const noexcept { return _mariaDB; }
    bool IsEncrypted() const;
    uint64 GetReconnectCount() const noexcept { return _reconnects.load(std::memory_order_relaxed); }
    MySQLConnectionInfo const& GetInfo() const noexcept { return _info; }

    bool TryLock() { return _mutex.try_lock(); }
    void Unlock() { _mutex.unlock(); }

    static bool IsConnectionLost(uint32 errorCode, bool mariaDB) noexcept;
    static bool IsSafeToRetry(uint32 errorCode, bool mariaDB) noexcept;
    static bool IsPermanentConnectError(uint32 errorCode) noexcept;
    static bool IsTransientLockError(uint32 errorCode) noexcept;

protected:
    virtual void DoPrepareStatements();
    void PrepareStatement(uint32 index, std::string_view name, std::string_view sql, ConnectionFlags flags);
    st_mysql* GetHandle() const noexcept { return _mysql; }
    bool RunQuery(std::string_view context, std::string_view sql, st_mysql_res** result, bool readOnly);
    bool AbandonTransaction();
    bool Reconnect();
    void ClearError() noexcept;

private:
    uint32 Connect(bool quiet);
    void CloseHandle();
    bool DrainResults(std::string_view context, std::string_view sql);
    void SetError(uint32 code, std::string text);
    bool PrepareOne(uint32 index, std::string const& name, std::string const& sql, bool quiet);
    bool PrepareRegistered();
    bool RunStatement(PreparedStatementBase const& values, bool readOnly, PreparedQueryResult* result);

    struct StatementRegistration
    {
        uint32 Index = 0;
        std::string Name;
        std::string Sql;
    };

    MySQLConnectionInfo _info;
    MySQLConnectionSettings _settings;
    st_mysql* _mysql = nullptr;
    std::mutex _mutex;
    uint32 _lastErrorCode = 0;
    uint64 _lastAffectedRows = 0;
    std::string _lastErrorText;
    std::atomic<uint64> _reconnects{ 0 };
    bool _mariaDB = false;
    bool _closed = true;
    bool _prepareFailed = false;
    std::chrono::steady_clock::time_point _unavailableUntil{};
    std::atomic<int> _activeUsers{ 0 };
    std::atomic<uint64> _concurrentUses{ 0 };
    std::vector<StatementRegistration> _registrations;
    std::vector<std::unique_ptr<MySQLPreparedStatement>> _statements;
};

#endif
