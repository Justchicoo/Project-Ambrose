/*
 * Project Ambrose by Imjustchico
 * Points each MYSQL_BIND at the matching parameter value with its connector type, signedness and length, and refuses to bind while any placeholder is unset.
 */

#include "MySQLPreparedStatement.h"

#include <mysql.h>

#include <fmt/format.h>

#include <cstring>
#include <type_traits>
#include <variant>

namespace
{
    char EmptyBuffer = 0;

    template<std::size_t Index, typename T>
    constexpr bool AlternativeIs = std::is_same_v<std::variant_alternative_t<Index, PreparedStatementValue>, T>;

    static_assert(AlternativeIs<0, std::monostate> && AlternativeIs<1, std::nullptr_t> && AlternativeIs<2, bool> && AlternativeIs<3, uint8> && AlternativeIs<4, uint16>
        && AlternativeIs<5, uint32> && AlternativeIs<6, uint64> && AlternativeIs<7, int8> && AlternativeIs<8, int16> && AlternativeIs<9, int32> && AlternativeIs<10, int64>
        && AlternativeIs<11, float> && AlternativeIs<12, double> && AlternativeIs<13, std::string> && AlternativeIs<14, std::vector<uint8>>);

    template<typename T>
    void BindNumber(MYSQL_BIND& bind, T const& value, enum_field_types type)
    {
        bind.buffer_type = type;
        bind.buffer = const_cast<void*>(static_cast<void const*>(&value));
        bind.buffer_length = sizeof(T);
        bind.is_unsigned = std::is_unsigned_v<T>;
    }
}

MySQLPreparedStatement::MySQLPreparedStatement(st_mysql_stmt* statement, uint32 index, std::string name, std::string sql)
    : _statement(statement), _index(index), _name(std::move(name)), _sql(std::move(sql)), _parameterCount(mysql_stmt_param_count(statement)),
    _binds(new MYSQL_BIND[_parameterCount ? _parameterCount : 1]), _lengths(_parameterCount), _flags(_parameterCount)
{
}

MySQLPreparedStatement::~MySQLPreparedStatement()
{
    if (_statement)
        mysql_stmt_close(_statement);
}

bool MySQLPreparedStatement::BindParameters(PreparedStatementBase const& values, std::string& error)
{
    std::vector<PreparedStatementValue> const& data = values.GetValues();
    if (data.size() != _parameterCount)
    {
        error = fmt::format("statement {} has {} placeholder(s) but {} value(s)", _name, _parameterCount, data.size());
        return false;
    }
    std::memset(_binds.get(), 0, sizeof(MYSQL_BIND) * (_parameterCount ? _parameterCount : 1));
    for (std::size_t i = 0; i < _parameterCount; ++i)
    {
        MYSQL_BIND& bind = _binds[i];
        PreparedStatementValue const& value = data[i];
        switch (value.index())
        {
            case 0:
                error = fmt::format("parameter {} not bound", i + 1);
                return false;
            case 1:
                bind.buffer_type = MYSQL_TYPE_NULL;
                break;
            case 2:
                _flags[i] = std::get<bool>(value) ? 1 : 0;
                BindNumber(bind, _flags[i], MYSQL_TYPE_TINY);
                break;
            case 3: BindNumber(bind, std::get<uint8>(value), MYSQL_TYPE_TINY); break;
            case 4: BindNumber(bind, std::get<uint16>(value), MYSQL_TYPE_SHORT); break;
            case 5: BindNumber(bind, std::get<uint32>(value), MYSQL_TYPE_LONG); break;
            case 6: BindNumber(bind, std::get<uint64>(value), MYSQL_TYPE_LONGLONG); break;
            case 7: BindNumber(bind, std::get<int8>(value), MYSQL_TYPE_TINY); break;
            case 8: BindNumber(bind, std::get<int16>(value), MYSQL_TYPE_SHORT); break;
            case 9: BindNumber(bind, std::get<int32>(value), MYSQL_TYPE_LONG); break;
            case 10: BindNumber(bind, std::get<int64>(value), MYSQL_TYPE_LONGLONG); break;
            case 11: BindNumber(bind, std::get<float>(value), MYSQL_TYPE_FLOAT); break;
            case 12: BindNumber(bind, std::get<double>(value), MYSQL_TYPE_DOUBLE); break;
            case 13:
            {
                std::string const& text = std::get<std::string>(value);
                bind.buffer_type = MYSQL_TYPE_STRING;
                bind.buffer = text.empty() ? &EmptyBuffer : const_cast<char*>(text.data());
                bind.buffer_length = static_cast<unsigned long>(text.size());
                _lengths[i] = static_cast<unsigned long>(text.size());
                bind.length = &_lengths[i];
                break;
            }
            case 14:
            {
                std::vector<uint8> const& bytes = std::get<std::vector<uint8>>(value);
                bind.buffer_type = MYSQL_TYPE_BLOB;
                bind.buffer = bytes.empty() ? static_cast<void*>(&EmptyBuffer) : static_cast<void*>(const_cast<uint8*>(bytes.data()));
                bind.buffer_length = static_cast<unsigned long>(bytes.size());
                _lengths[i] = static_cast<unsigned long>(bytes.size());
                bind.length = &_lengths[i];
                break;
            }
            default:
                error = fmt::format("statement {} parameter {} has an unsupported type", _name, i + 1);
                return false;
        }
    }
    if (_parameterCount && mysql_stmt_bind_param(_statement, _binds.get()))
    {
        error = fmt::format("binding statement {} failed: [{}] {}", _name, mysql_stmt_errno(_statement), mysql_stmt_error(_statement));
        return false;
    }
    return true;
}
