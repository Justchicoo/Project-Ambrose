/*
 * Project Ambrose by Imjustchico
 * A server-side prepared statement handle: binds a PreparedStatementBase's values to MYSQL_BIND entries after checking every placeholder is set, and keeps its name and SQL for logs.
 */

#ifndef AMBROSE_MYSQLPREPAREDSTATEMENT_H
#define AMBROSE_MYSQLPREPAREDSTATEMENT_H

#include "PreparedStatement.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct st_mysql_stmt;
struct st_mysql_bind;

class MySQLPreparedStatement
{
public:
    MySQLPreparedStatement(st_mysql_stmt* statement, uint32 index, std::string name, std::string sql);
    ~MySQLPreparedStatement();

    MySQLPreparedStatement(MySQLPreparedStatement const&) = delete;
    MySQLPreparedStatement& operator=(MySQLPreparedStatement const&) = delete;

    bool BindParameters(PreparedStatementBase const& values, std::string& error);

    st_mysql_stmt* GetHandle() const noexcept { return _statement; }
    uint32 GetIndex() const noexcept { return _index; }
    std::string const& GetName() const noexcept { return _name; }
    std::string const& GetSql() const noexcept { return _sql; }
    std::size_t GetParameterCount() const noexcept { return _parameterCount; }

private:
    st_mysql_stmt* _statement;
    uint32 _index;
    std::string _name;
    std::string _sql;
    std::size_t _parameterCount;
    std::unique_ptr<st_mysql_bind[]> _binds;
    std::vector<unsigned long> _lengths;
    std::vector<uint8> _flags;
};

#endif
