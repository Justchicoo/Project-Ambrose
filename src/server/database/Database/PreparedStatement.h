/*
 * Project Ambrose by Imjustchico
 * A registered statement's id plus its typed parameter values, set by index and bound when it runs, and the queued tasks that run one on an async connection: one that carries back rows read, and one that carries back how many rows were changed.
 */

#ifndef AMBROSE_PREPAREDSTATEMENT_H
#define AMBROSE_PREPAREDSTATEMENT_H

#include "DatabaseEnvFwd.h"
#include "SQLOperation.h"
#include "Types.h"

#include <cstddef>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

using PreparedStatementValue = std::variant<std::monostate, std::nullptr_t, bool, uint8, uint16, uint32, uint64, int8, int16, int32, int64, float, double, std::string, std::vector<uint8>>;

class PreparedStatementBase
{
public:
    PreparedStatementBase(uint32 index, std::size_t parameterCount);
    virtual ~PreparedStatementBase() = default;

    template<typename T>
        requires std::is_constructible_v<PreparedStatementValue, T>
    void SetData(std::size_t index, T&& value)
    {
        if (!CheckIndex(index))
            return;
        _values[index] = PreparedStatementValue(std::forward<T>(value));
    }

    void SetData(std::size_t index, char const* value);
    void SetData(std::size_t index, std::string_view value);
    void Clear();

    uint32 GetIndex() const noexcept { return _index; }
    std::size_t GetParameterCount() const noexcept { return _values.size(); }
    std::vector<PreparedStatementValue> const& GetValues() const noexcept { return _values; }
    std::string DescribeValues() const;

private:
    bool CheckIndex(std::size_t index) const;

    uint32 _index;
    std::vector<PreparedStatementValue> _values;
};

class PreparedStatementTask : public SQLOperation
{
public:
    PreparedStatementTask(std::unique_ptr<PreparedStatementBase> statement, bool hasResult);

    std::future<PreparedQueryResult> GetFuture() { return _result.get_future(); }

    void Execute(MySQLConnection& connection) override;
    void Cancel() override;

private:
    std::unique_ptr<PreparedStatementBase> _statement;
    bool _hasResult;
    std::promise<PreparedQueryResult> _result;
};

class CountedStatementTask : public SQLOperation
{
public:
    explicit CountedStatementTask(std::unique_ptr<PreparedStatementBase> statement);

    std::future<std::optional<uint64>> GetFuture() { return _result.get_future(); }

    void Execute(MySQLConnection& connection) override;
    void Cancel() override;

private:
    std::unique_ptr<PreparedStatementBase> _statement;
    std::promise<std::optional<uint64>> _result;
};

template<typename ConnectionType>
class PreparedStatement : public PreparedStatementBase
{
public:
    PreparedStatement(uint32 index, std::size_t parameterCount) : PreparedStatementBase(index, parameterCount)
    {
    }
};

#endif
