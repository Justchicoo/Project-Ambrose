/*
 * Project Ambrose by Imjustchico
 * Opens connector handles with utf8mb4, timeouts, TLS and the plugin folder, prepares registered statements, runs text and prepared queries and drains extra results, logs errors to sql.sql, and reconnects with backoff, re-preparing statements and retrying only work that cannot have run twice.
 */

#include "MySQLConnection.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "Log.h"
#include "MySQLPreparedStatement.h"
#include "PreparedStatement.h"
#include "QueryResult.h"
#include "StringUtil.h"

#include <errmsg.h>
#include <mysql.h>

#include <algorithm>
#include <filesystem>
#include <thread>
#include <vector>

namespace
{
    constexpr uint32 ErrorDatabaseAccessDenied = 1044;
    constexpr uint32 ErrorAccessDenied = 1045;
    constexpr uint32 ErrorEmptyQuery = 1065;
    constexpr uint32 ErrorBadDatabase = 1049;
    constexpr uint32 ErrorHostNotAllowed = 1130;
    constexpr uint32 ErrorAuthProtocol = 1251;
    constexpr uint32 MariaDBConnectionKilled = 1927;
    constexpr uint32 MySQLClientInteractionTimeout = 4031;

    void InitializeLibrary()
    {
        static std::once_flag once;
        std::call_once(once, [] { mysql_library_init(0, nullptr, nullptr); });
    }

    std::string const& GetPluginDirectory()
    {
        static std::string const directory = []
        {
            std::error_code error;
            std::filesystem::path const plugins = Ambrose::GetExecutableDirectory() / "plugins" / "libmariadb";
            return std::filesystem::is_directory(plugins, error) ? ConfigMgr::PathToUtf8(plugins) : std::string();
        }();
        return directory;
    }
}

std::optional<MySQLConnectionInfo> MySQLConnectionInfo::Parse(std::string_view text, std::string* error)
{
    auto fail = [error](std::string message) -> std::optional<MySQLConnectionInfo>
    {
        if (error)
            *error = std::move(message);
        return std::nullopt;
    };

    std::vector<std::string_view> const tokens = Ambrose::Tokenize(text, ';', true);
    if (tokens.size() < 5 || tokens.size() > 7)
        return fail(fmt::format("expected 'host;port;user;password;database[;tls[;ca file]]' but found {} field(s)", tokens.size()));

    MySQLConnectionInfo info;
    info.Host = std::string(Ambrose::Trim(tokens[0]));
    std::string_view const port = Ambrose::Trim(tokens[1]);
    if (info.Host.empty())
        return fail("the host is empty");
    if (info.Host == ".")
    {
        if (port.empty())
            return fail("a socket or pipe name must follow host '.'");
        info.Socket = std::string(port);
        info.Port = 0;
    }
    else
    {
        std::optional<uint16> const number = Ambrose::StringTo<uint16>(port);
        if (!number)
            return fail(fmt::format("port '{}' is not a number from 0 to 65535", port));
        info.Port = *number;
    }
    info.User = std::string(Ambrose::Trim(tokens[2]));
    info.Password = std::string(tokens[3]);
    info.Database = std::string(Ambrose::Trim(tokens[4]));
    if (tokens.size() >= 6)
    {
        std::string_view const tls = Ambrose::Trim(tokens[5]);
        if (tls.empty() || Ambrose::EqualsIgnoreCase(tls, "off"))
            info.Tls = DatabaseTls::Off;
        else if (Ambrose::EqualsIgnoreCase(tls, "tls") || Ambrose::EqualsIgnoreCase(tls, "ssl"))
            info.Tls = DatabaseTls::Required;
        else if (Ambrose::EqualsIgnoreCase(tls, "tls-verify") || Ambrose::EqualsIgnoreCase(tls, "ssl-verify"))
            info.Tls = DatabaseTls::RequiredVerified;
        else
            return fail(fmt::format("TLS mode '{}' is not off, tls, or tls-verify", tls));
    }
    if (tokens.size() == 7)
    {
        info.TlsCa = std::string(Ambrose::Trim(tokens[6]));
        if (!info.TlsCa.empty() && info.Tls == DatabaseTls::Off)
            return fail("a CA file needs TLS mode tls or tls-verify");
    }
    return info;
}

std::string MySQLConnectionInfo::ToLogString() const
{
    if (!Socket.empty())
        return fmt::format("{}@{}/{}", User, Socket, Database);
    return fmt::format("{}@{}:{}/{}", User, Host, Port, Database);
}

MySQLConnection::MySQLConnection(MySQLConnectionInfo info, MySQLConnectionSettings settings) : _info(std::move(info)), _settings(settings)
{
}

MySQLConnection::~MySQLConnection()
{
    Close();
}

bool MySQLConnection::IsConnectionLost(uint32 errorCode, bool mariaDB) noexcept
{
    switch (errorCode)
    {
        case CR_SERVER_GONE_ERROR:
        case CR_SERVER_LOST:
        case CR_SERVER_LOST_EXTENDED:
        case CR_COMMANDS_OUT_OF_SYNC:
        case CR_NEW_STMT_METADATA:
            return true;
        case MariaDBConnectionKilled:
            return mariaDB;
        case MySQLClientInteractionTimeout:
            return !mariaDB;
        default:
            return false;
    }
}

bool MySQLConnection::IsSafeToRetry(uint32 errorCode, bool mariaDB) noexcept
{
    switch (errorCode)
    {
        case CR_SERVER_GONE_ERROR:
        case CR_COMMANDS_OUT_OF_SYNC:
        case CR_NEW_STMT_METADATA:
            return true;
        case MariaDBConnectionKilled:
            return mariaDB;
        case MySQLClientInteractionTimeout:
            return !mariaDB;
        default:
            return false;
    }
}

bool MySQLConnection::IsPermanentConnectError(uint32 errorCode) noexcept
{
    switch (errorCode)
    {
        case ErrorDatabaseAccessDenied:
        case ErrorAccessDenied:
        case ErrorBadDatabase:
        case ErrorHostNotAllowed:
        case ErrorAuthProtocol:
        case CR_AUTH_PLUGIN_CANNOT_LOAD:
        case CR_SSL_CONNECTION_ERROR:
            return true;
        default:
            return false;
    }
}

void MySQLConnection::CloseHandle()
{
    if (_mysql)
    {
        mysql_close(_mysql);
        _mysql = nullptr;
    }
    _statements.clear();
}

uint32 MySQLConnection::Open()
{
    CloseHandle();
    _closed = false;
    uint32 code = Connect(false);
    if (code == 0 && !PrepareRegistered())
    {
        code = _lastErrorCode;
        CloseHandle();
    }
    if (code != 0)
        _closed = true;
    return code;
}

void MySQLConnection::Close()
{
    _closed = true;
    CloseHandle();
}

void MySQLConnection::ClearError() noexcept
{
    _lastErrorCode = 0;
    _lastErrorText.clear();
}

void MySQLConnection::SetError(uint32 code, std::string text)
{
    _lastErrorCode = code;
    _lastErrorText = std::move(text);
}

uint32 MySQLConnection::Connect(bool quiet)
{
    InitializeLibrary();
    MYSQL* const mysql = mysql_init(nullptr);
    if (!mysql)
    {
        SetError(CR_OUT_OF_MEMORY, "mysql_init failed");
        LOG_ERROR("sql.sql", "Could not create a connection handle for {}", _info.ToLogString());
        return _lastErrorCode;
    }

    unsigned int const connectTimeout = static_cast<unsigned int>(_settings.ConnectTimeout.count());
    unsigned int const readTimeout = static_cast<unsigned int>(_settings.ReadTimeout.count());
    unsigned int const writeTimeout = static_cast<unsigned int>(_settings.WriteTimeout.count());
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeout);
    mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &readTimeout);
    mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &writeTimeout);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    my_bool const enforce = _info.Tls != DatabaseTls::Off;
    my_bool const verify = _info.Tls == DatabaseTls::RequiredVerified;
    mysql_optionsv(mysql, MYSQL_OPT_SSL_ENFORCE, &enforce);
    mysql_optionsv(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
    if (!_info.TlsCa.empty())
        mysql_optionsv(mysql, MYSQL_OPT_SSL_CA, _info.TlsCa.c_str());
    if (!GetPluginDirectory().empty())
        mysql_optionsv(mysql, MYSQL_PLUGIN_DIR, GetPluginDirectory().c_str());

    char const* host = _info.Host.c_str();
    char const* socket = nullptr;
    if (!_info.Socket.empty())
    {
        socket = _info.Socket.c_str();
#ifdef _WIN32
        unsigned int const protocol = MYSQL_PROTOCOL_PIPE;
        mysql_optionsv(mysql, MYSQL_OPT_PROTOCOL, &protocol);
#else
        host = "localhost";
#endif
    }

    char const* const database = _info.Database.empty() ? nullptr : _info.Database.c_str();
    if (!mysql_real_connect(mysql, host, _info.User.c_str(), _info.Password.c_str(), database, _info.Port, socket, 0))
    {
        SetError(mysql_errno(mysql), mysql_error(mysql));
        mysql_close(mysql);
        if (!quiet)
            LOG_ERROR("sql.sql", "Could not connect to {}: [{}] {}", _info.ToLogString(), _lastErrorCode, _lastErrorText);
        return _lastErrorCode;
    }
    if (_info.Tls != DatabaseTls::Off && !mysql_get_ssl_cipher(mysql))
    {
        mysql_close(mysql);
        SetError(CR_SSL_CONNECTION_ERROR, "TLS is required but the server did not encrypt the connection");
        LOG_ERROR("sql.sql", "Could not connect to {}: [{}] {}", _info.ToLogString(), _lastErrorCode, _lastErrorText);
        return _lastErrorCode;
    }

    mysql_autocommit(mysql, 1);
    _mysql = mysql;
    _mariaDB = mariadb_connection(mysql) != 0;
    ClearError();
    if (!quiet)
        LOG_INFO("sql.driver", "Connected to {} running {}{}", _info.ToLogString(), mysql_get_server_info(_mysql), IsEncrypted() ? " over TLS" : "");
    return 0;
}

bool MySQLConnection::DrainResults(std::string_view context, std::string_view sql)
{
    while (mysql_more_results(_mysql))
    {
        int const status = mysql_next_result(_mysql);
        if (status == -1)
            break;
        if (status > 0)
        {
            SetError(mysql_errno(_mysql), mysql_error(_mysql));
            LOG_ERROR("sql.sql", "{} on {} failed in a later result: [{}] {} in: {}", context, _info.ToLogString(), _lastErrorCode, _lastErrorText, sql);
            return false;
        }
        if (MYSQL_RES* const extra = mysql_store_result(_mysql))
            mysql_free_result(extra);
        else if (mysql_field_count(_mysql) != 0)
        {
            SetError(mysql_errno(_mysql), mysql_error(_mysql));
            LOG_ERROR("sql.sql", "{} on {} could not read a later result: [{}] {} in: {}", context, _info.ToLogString(), _lastErrorCode, _lastErrorText, sql);
            return false;
        }
    }
    return true;
}

bool MySQLConnection::RunQuery(std::string_view context, std::string_view sql, st_mysql_res** result, bool readOnly)
{
    ClearError();
    if (sql.empty())
    {
        SetError(ErrorEmptyQuery, "the statement is empty");
        LOG_ERROR("sql.sql", "{} on {} refused an empty statement", context, _info.ToLogString());
        return false;
    }
    if (_closed)
    {
        SetError(CR_SERVER_GONE_ERROR, "the connection is closed");
        LOG_ERROR("sql.sql", "{} on {} refused: the connection is closed", context, _info.ToLogString());
        return false;
    }

    for (int attempt = 0;; ++attempt)
    {
        ClearError();
        if (!_mysql && !Reconnect())
            return false;
        bool const inTransaction = (_mysql->server_status & SERVER_STATUS_IN_TRANS) != 0;
        if (mysql_real_query(_mysql, sql.data(), static_cast<unsigned long>(sql.size())) == 0)
        {
            MYSQL_RES* const stored = mysql_store_result(_mysql);
            if (stored || mysql_field_count(_mysql) == 0)
            {
                if (!DrainResults(context, sql))
                {
                    if (stored)
                        mysql_free_result(stored);
                    return false;
                }
                if (result)
                    *result = stored;
                else if (stored)
                    mysql_free_result(stored);
                return true;
            }
        }

        uint32 const code = mysql_errno(_mysql);
        std::string const text = mysql_error(_mysql);
        SetError(code, text);
        if (!IsConnectionLost(code, _mariaDB))
        {
            LOG_ERROR("sql.sql", "{} on {} failed: [{}] {} in: {}", context, _info.ToLogString(), code, text, sql);
            return false;
        }

        bool const safe = IsSafeToRetry(code, _mariaDB) || (readOnly && (code == CR_SERVER_LOST || code == CR_SERVER_LOST_EXTENDED));
        bool const retry = attempt == 0 && !inTransaction && safe;
        LOG_WARN("sql.sql", "Reconnecting to {} after {} failed: [{}] {}{}", _info.ToLogString(), context, code, text,
            inTransaction ? "; the open transaction was rolled back" : retry ? "; retrying after reconnecting" : "; the statement is not retried");
        bool const reconnected = Reconnect();
        SetError(code, text);
        if (!reconnected || !retry)
            return false;
    }
}

bool MySQLConnection::Execute(std::string_view sql)
{
    return RunQuery("Execute", sql, nullptr, false);
}

QueryResult MySQLConnection::Query(std::string_view sql)
{
    MYSQL_RES* result = nullptr;
    if (!RunQuery("Query", sql, &result, true) || !result)
        return nullptr;
    uint64 const rows = mysql_num_rows(result);
    uint32 const fields = mysql_num_fields(result);
    if (rows == 0)
    {
        mysql_free_result(result);
        return nullptr;
    }
    QueryResult set = std::make_shared<ResultSet>(result, rows, fields);
    set->NextRow();
    return set;
}

std::string MySQLConnection::Escape(std::string_view text)
{
    std::string escaped(text.size() * 2 + 1, '\0');
    unsigned long const length = _mysql
        ? mysql_real_escape_string(_mysql, escaped.data(), text.data(), static_cast<unsigned long>(text.size()))
        : mysql_escape_string(escaped.data(), text.data(), static_cast<unsigned long>(text.size()));
    escaped.resize(length);
    return escaped;
}

bool MySQLConnection::Ping()
{
    ClearError();
    if (_closed)
        return false;
    if (!_mysql)
        return Reconnect();
    if (mysql_ping(_mysql) == 0)
        return true;
    uint32 const code = mysql_errno(_mysql);
    SetError(code, mysql_error(_mysql));
    if (IsConnectionLost(code, _mariaDB))
        return Reconnect();
    LOG_ERROR("sql.sql", "Ping on {} failed: [{}] {}", _info.ToLogString(), code, _lastErrorText);
    return false;
}

std::string MySQLConnection::GetServerInfo() const
{
    return _mysql ? mysql_get_server_info(_mysql) : "";
}

uint64 MySQLConnection::GetServerVersion() const
{
    return _mysql ? mysql_get_server_version(_mysql) : 0;
}

uint64 MySQLConnection::GetThreadId() const
{
    return _mysql ? mysql_thread_id(_mysql) : 0;
}

bool MySQLConnection::IsEncrypted() const
{
    return _mysql && mysql_get_ssl_cipher(_mysql) != nullptr;
}

bool MySQLConnection::Reconnect()
{
    if (_closed)
        return false;
    CloseHandle();
    auto const start = std::chrono::steady_clock::now();
    std::chrono::milliseconds delay = _settings.FirstReconnectDelay;
    for (uint32 attempt = 1;; ++attempt)
    {
        uint32 code = Connect(true);
        if (code == 0)
        {
            if (PrepareRegistered())
            {
                ++_reconnects;
                LOG_INFO("sql.driver", "Reconnected to {} after {} attempt(s)", _info.ToLogString(), attempt);
                return true;
            }
            code = _lastErrorCode;
            CloseHandle();
        }
        LOG_WARN("sql.driver", "Reconnect attempt {} to {} failed: [{}] {}", attempt, _info.ToLogString(), code, _lastErrorText);
        if (IsPermanentConnectError(code) || std::chrono::steady_clock::now() - start + delay > _settings.GiveUpReconnectAfter)
        {
            LOG_ERROR("sql.driver", "Gave up reconnecting to {} after {} attempt(s): [{}] {}", _info.ToLogString(), attempt, code, _lastErrorText);
            return false;
        }
        std::this_thread::sleep_for(delay);
        delay = std::min(delay * 2, _settings.MaxReconnectDelay);
    }
}

void MySQLConnection::DoPrepareStatements()
{
}

void MySQLConnection::PrepareStatement(uint32 index, std::string_view name, std::string_view sql, ConnectionFlags flags)
{
    if ((static_cast<uint8>(flags) & static_cast<uint8>(_settings.Flags)) == 0)
        return;
    for (StatementRegistration const& existing : _registrations)
    {
        if (existing.Index == index)
        {
            LOG_ERROR("sql.sql", "Could not prepare statement {}: index {} is already used by {}", name, index, existing.Name);
            _prepareFailed = true;
            return;
        }
    }
    StatementRegistration registration{ index, std::string(name), std::string(sql) };
    if (!PrepareOne(index, registration.Name, registration.Sql, false))
    {
        _prepareFailed = true;
        return;
    }
    _registrations.push_back(std::move(registration));
}

bool MySQLConnection::PrepareStatements()
{
    _registrations.clear();
    _statements.clear();
    _prepareFailed = false;
    if (!_mysql)
    {
        SetError(CR_SERVER_GONE_ERROR, "the connection is not open");
        LOG_ERROR("sql.sql", "Cannot prepare statements on {}: the connection is not open", _info.ToLogString());
        return false;
    }
    DoPrepareStatements();
    return !_prepareFailed;
}

bool MySQLConnection::PrepareOne(uint32 index, std::string const& name, std::string const& sql, bool quiet)
{
    MYSQL_STMT* const handle = mysql_stmt_init(_mysql);
    if (!handle)
    {
        SetError(mysql_errno(_mysql), mysql_error(_mysql));
        if (!quiet)
            LOG_ERROR("sql.sql", "Could not prepare statement {}: [{}] {}", name, _lastErrorCode, _lastErrorText);
        return false;
    }
    if (mysql_stmt_prepare(handle, sql.data(), static_cast<unsigned long>(sql.size())))
    {
        SetError(mysql_stmt_errno(handle), mysql_stmt_error(handle));
        mysql_stmt_close(handle);
        if (!quiet)
            LOG_ERROR("sql.sql", "Could not prepare statement {}: [{}] {} in: {}", name, _lastErrorCode, _lastErrorText, sql);
        return false;
    }
    if (_statements.size() <= index)
        _statements.resize(index + 1);
    _statements[index] = std::make_unique<MySQLPreparedStatement>(handle, index, name, sql);
    return true;
}

bool MySQLConnection::PrepareRegistered()
{
    _statements.clear();
    for (StatementRegistration const& registration : _registrations)
        if (!PrepareOne(registration.Index, registration.Name, registration.Sql, false) && (!_mysql || IsConnectionLost(_lastErrorCode, _mariaDB)))
            return false;
    ClearError();
    return true;
}

bool MySQLConnection::IsStatementPrepared(uint32 index) const noexcept
{
    return index < _statements.size() && _statements[index] != nullptr;
}

std::size_t MySQLConnection::GetPreparedStatementCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(_statements.begin(), _statements.end(), [](auto const& statement) { return statement != nullptr; }));
}

std::unique_ptr<PreparedStatementBase> MySQLConnection::GetPreparedStatement(uint32 index) const
{
    if (!IsStatementPrepared(index))
    {
        LOG_ERROR("sql.sql", "Statement {} is not prepared on {}", index, _info.ToLogString());
        return nullptr;
    }
    return std::make_unique<PreparedStatementBase>(index, _statements[index]->GetParameterCount());
}

bool MySQLConnection::Execute(PreparedStatementBase const& statement)
{
    return RunStatement(statement, false, nullptr);
}

PreparedQueryResult MySQLConnection::Query(PreparedStatementBase const& statement)
{
    PreparedQueryResult result;
    if (!RunStatement(statement, true, &result))
        return nullptr;
    return result;
}

bool MySQLConnection::RunStatement(PreparedStatementBase const& values, bool readOnly, PreparedQueryResult* result)
{
    ClearError();
    if (_closed)
    {
        SetError(CR_SERVER_GONE_ERROR, "the connection is closed");
        LOG_ERROR("sql.sql", "Statement {} on {} refused: the connection is closed", values.GetIndex(), _info.ToLogString());
        return false;
    }

    for (int attempt = 0;; ++attempt)
    {
        ClearError();
        if (!_mysql && !Reconnect())
            return false;
        if (!IsStatementPrepared(values.GetIndex()))
        {
            SetError(CR_UNKNOWN_ERROR, fmt::format("statement {} is not prepared on this connection", values.GetIndex()));
            LOG_ERROR("sql.sql", "Could not run statement {} on {}: it is not prepared on this connection", values.GetIndex(), _info.ToLogString());
            return false;
        }
        MySQLPreparedStatement& prepared = *_statements[values.GetIndex()];
        std::string bindError;
        if (!prepared.BindParameters(values, bindError))
        {
            SetError(CR_PARAMS_NOT_BOUND, bindError);
            LOG_ERROR("sql.sql", "Could not run statement {} on {}: {}", prepared.GetName(), _info.ToLogString(), bindError);
            return false;
        }

        bool const inTransaction = (_mysql->server_status & SERVER_STATUS_IN_TRANS) != 0;
        MYSQL_STMT* const handle = prepared.GetHandle();
        uint32 code = 0;
        std::string text;
        if (mysql_stmt_execute(handle) == 0)
        {
            PreparedQueryResult loaded = PreparedResultSet::Load(handle, code, text);
            while (code == 0 && mysql_stmt_more_results(handle))
            {
                int const status = mysql_stmt_next_result(handle);
                if (status > 0)
                {
                    code = mysql_stmt_errno(handle);
                    text = mysql_stmt_error(handle);
                }
                else if (status == 0)
                    mysql_stmt_free_result(handle);
                else
                    break;
            }
            if (code == 0)
            {
                if (result)
                    *result = std::move(loaded);
                return true;
            }
        }
        else
        {
            code = mysql_stmt_errno(handle);
            text = mysql_stmt_error(handle);
        }

        SetError(code, text);
        if (!IsConnectionLost(code, _mariaDB))
        {
            LOG_ERROR("sql.sql", "Statement {} on {} failed: [{}] {} with values ({})", prepared.GetName(), _info.ToLogString(), code, text, values.DescribeValues());
            return false;
        }
        std::string const name = prepared.GetName();
        bool const safe = IsSafeToRetry(code, _mariaDB) || (readOnly && (code == CR_SERVER_LOST || code == CR_SERVER_LOST_EXTENDED));
        bool const retry = attempt == 0 && !inTransaction && safe;
        LOG_WARN("sql.sql", "Reconnecting to {} after statement {} failed: [{}] {}{}", _info.ToLogString(), name, code, text,
            inTransaction ? "; the open transaction was rolled back" : retry ? "; retrying after reconnecting" : "; the statement is not retried");
        bool const reconnected = Reconnect();
        SetError(code, text);
        if (!reconnected || !retry)
            return false;
    }
}
