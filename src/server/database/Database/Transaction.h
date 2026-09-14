/*
 * Project Ambrose by Imjustchico
 * An ordered batch of SQL text and prepared statements committed atomically, the task that commits it with deadlock retries, and the callback that reports the outcome.
 */

#ifndef AMBROSE_TRANSACTION_H
#define AMBROSE_TRANSACTION_H

#include "PreparedStatement.h"
#include "SQLOperation.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class TransactionBase
{
public:
    using Entry = std::variant<std::string, std::unique_ptr<PreparedStatementBase>>;

    TransactionBase() = default;
    virtual ~TransactionBase() = default;

    TransactionBase(TransactionBase const&) = delete;
    TransactionBase& operator=(TransactionBase const&) = delete;

    void Append(std::string sql);
    std::size_t GetSize() const noexcept { return _entries.size(); }
    std::vector<Entry> const& GetEntries() const noexcept { return _entries; }
    bool IsValid() const noexcept { return _valid; }
    bool Lock() noexcept { return !_locked.exchange(true); }

protected:
    void AppendStatement(std::unique_ptr<PreparedStatementBase> statement);

private:
    bool CheckEditable();

    std::vector<Entry> _entries;
    bool _valid = true;
    std::atomic<bool> _locked{ false };
};

template<typename ConnectionType>
class Transaction : public TransactionBase
{
public:
    using TransactionBase::Append;

    void Append(std::unique_ptr<PreparedStatement<ConnectionType>> statement)
    {
        AppendStatement(std::move(statement));
    }
};

class TransactionTask : public SQLOperation
{
public:
    static constexpr std::chrono::seconds DeadlockRetryLimit{ 60 };

    explicit TransactionTask(std::shared_ptr<TransactionBase> transaction);

    std::future<bool> GetFuture() { return _result.get_future(); }

    void Execute(MySQLConnection& connection) override;
    void Cancel() override;

    static bool Commit(MySQLConnection& connection, TransactionBase const& transaction, std::chrono::milliseconds retryLimit = DeadlockRetryLimit);

private:
    std::shared_ptr<TransactionBase> _transaction;
    std::promise<bool> _result;
};

class TransactionCallback
{
public:
    explicit TransactionCallback(std::future<bool>&& result);

    TransactionCallback(TransactionCallback&&) noexcept = default;
    TransactionCallback& operator=(TransactionCallback&&) noexcept = default;

    TransactionCallback&& AfterComplete(std::function<void(bool)>&& callback);
    bool IsReady() const;
    bool InvokeIfReady();

private:
    std::future<bool> _result;
    std::function<void(bool)> _callback;
    bool _finished = false;
};

#endif
