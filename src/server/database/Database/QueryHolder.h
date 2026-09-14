/*
 * Project Ambrose by Imjustchico
 * A batch of prepared queries that run together on one connection, such as everything a character login loads, with each result kept by slot, and the task that queues it.
 */

#ifndef AMBROSE_QUERYHOLDER_H
#define AMBROSE_QUERYHOLDER_H

#include "DatabaseEnvFwd.h"
#include "PreparedStatement.h"
#include "SQLOperation.h"

#include <cstddef>
#include <future>
#include <memory>
#include <vector>

class SQLQueryHolderBase
{
public:
    explicit SQLQueryHolderBase(std::size_t slots);
    virtual ~SQLQueryHolderBase();

    SQLQueryHolderBase(SQLQueryHolderBase const&) = delete;
    SQLQueryHolderBase& operator=(SQLQueryHolderBase const&) = delete;

    std::size_t GetSlotCount() const noexcept { return _slots.size(); }
    PreparedQueryResult GetPreparedResult(std::size_t slot) const;
    void Run(MySQLConnection& connection);

protected:
    bool SetPreparedQueryBase(std::size_t slot, std::unique_ptr<PreparedStatementBase> statement);

private:
    struct Slot
    {
        std::unique_ptr<PreparedStatementBase> Statement;
        PreparedQueryResult Result;
    };

    std::vector<Slot> _slots;
};

class QueryHolderTask : public SQLOperation
{
public:
    explicit QueryHolderTask(std::shared_ptr<SQLQueryHolderBase> holder);

    std::future<void> GetFuture() { return _done.get_future(); }

    void Execute(MySQLConnection& connection) override;
    void Cancel() override;

private:
    std::shared_ptr<SQLQueryHolderBase> _holder;
    std::promise<void> _done;
};

template<typename ConnectionType>
class SQLQueryHolder : public SQLQueryHolderBase
{
public:
    using SQLQueryHolderBase::SQLQueryHolderBase;

    bool SetPreparedQuery(std::size_t slot, std::unique_ptr<PreparedStatement<ConnectionType>> statement)
    {
        return SetPreparedQueryBase(slot, std::move(statement));
    }
};

#endif
