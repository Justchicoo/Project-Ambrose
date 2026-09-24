/*
 * Project Ambrose by Imjustchico
 * Buffers log rows with drop-oldest overflow, wakes every flush interval or full batch, commits each batch as one LOGIN_INS_LOG transaction when the login pool can take it, and reports drops at most every ten seconds.
 */

#include "DatabaseLogSink.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ThreadName.h"
#include "Utf.h"

#include <algorithm>

DatabaseLogSink& DatabaseLogSink::Instance()
{
    static DatabaseLogSink instance;
    return instance;
}

std::string DatabaseLogSink::PrepareText(std::string_view text, std::size_t maxLength)
{
    std::string prepared;
    if (Utf::IsValidUtf8(text))
        prepared.assign(text);
    else
    {
        std::optional<std::u16string> const utf16 = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::ReplaceWithU_FFFD);
        std::optional<std::string> const repaired = utf16 ? Utf::Utf16ToUtf8(*utf16, Utf::InvalidPolicy::ReplaceWithU_FFFD) : std::nullopt;
        prepared = repaired ? *repaired : std::string();
    }
    if (prepared.size() > maxLength)
    {
        std::size_t cut = maxLength;
        while (cut > 0 && (static_cast<unsigned char>(prepared[cut]) & 0xC0) == 0x80)
            --cut;
        prepared.resize(cut);
    }
    return prepared;
}

void DatabaseLogSink::Start()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_running.exchange(true))
        return;
    _stopRequested = false;
    _warnedNoAsync = false;
    _thread = std::thread([this] { Run(); });
}

void DatabaseLogSink::Stop(std::chrono::milliseconds drainTimeout)
{
    std::thread thread;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_running.load())
            return;
        _stopRequested = true;
        _flushRequested = true;
        thread = std::move(_thread);
    }
    _wake.notify_all();
    if (thread.joinable())
        thread.join();
    auto const deadline = std::chrono::steady_clock::now() + drainTimeout;
    std::deque<Row> remaining;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        remaining.swap(_rows);
        _running = false;
    }
    while (!remaining.empty() && std::chrono::steady_clock::now() < deadline)
    {
        std::deque<Row> batch;
        while (!remaining.empty() && batch.size() < BatchSize)
        {
            batch.push_back(std::move(remaining.front()));
            remaining.pop_front();
        }
        if (!CommitBatch(batch))
            break;
    }
    if (!remaining.empty())
        _dropped.fetch_add(remaining.size());
}

void DatabaseLogSink::Push(Row row)
{
    row.Category = PrepareText(row.Category, MaxCategoryLength);
    row.Message = PrepareText(row.Message, MaxMessageLength);
    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_running.load())
        {
            _dropped.fetch_add(1);
            return;
        }
        if (_rows.size() >= _capacity.load())
        {
            _rows.pop_front();
            _dropped.fetch_add(1);
        }
        _rows.push_back(std::move(row));
        wake = _rows.size() >= BatchSize;
    }
    if (wake)
        _wake.notify_one();
}

void DatabaseLogSink::Flush()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _flushRequested = true;
    }
    _wake.notify_one();
}

std::size_t DatabaseLogSink::GetPendingCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _rows.size();
}

bool DatabaseLogSink::CommitBatch(std::deque<Row>& batch)
{
    if (batch.empty())
        return true;
    if (!LoginDatabase.IsOpen() || LoginDatabase.GetAsyncConnectionCount() == 0)
    {
        if (LoginDatabase.IsOpen() && !_warnedNoAsync)
        {
            _warnedNoAsync = true;
            LOG_WARN("server.logging", "The DB log appender needs at least one async login database connection (LoginDatabase.WorkerThreads); log rows are dropped");
        }
        _dropped.fetch_add(batch.size());
        batch.clear();
        return false;
    }
    auto transaction = LoginDatabase.BeginTransaction();
    std::size_t const count = batch.size();
    for (Row& row : batch)
    {
        auto statement = LoginDatabase.GetPreparedStatement(LOGIN_INS_LOG);
        if (!statement)
        {
            _dropped.fetch_add(count);
            batch.clear();
            return false;
        }
        statement->SetData(0, row.LoggedAtMs);
        statement->SetData(1, row.RealmId);
        statement->SetData(2, std::move(row.Category));
        statement->SetData(3, row.Level);
        statement->SetData(4, std::move(row.Message));
        transaction->Append(std::move(statement));
    }
    batch.clear();
    LoginDatabase.CommitTransaction(std::move(transaction));
    _written.fetch_add(count);
    return true;
}

void DatabaseLogSink::Run()
{
    Ambrose::Threading::SetCurrentThreadName("DB log sink");
    auto lastReport = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(_mutex);
    while (true)
    {
        _wake.wait_for(lock, FlushInterval, [this] { return _stopRequested || _flushRequested || _rows.size() >= BatchSize; });
        if (_stopRequested)
            return;
        _flushRequested = false;
        while (!_rows.empty() && !_stopRequested)
        {
            if (LoginDatabase.IsOpen() && LoginDatabase.GetQueueSize() > BusyQueueThreshold)
                break;
            std::deque<Row> batch;
            std::size_t const take = std::min(_rows.size(), BatchSize);
            for (std::size_t i = 0; i < take; ++i)
            {
                batch.push_back(std::move(_rows.front()));
                _rows.pop_front();
            }
            lock.unlock();
            CommitBatch(batch);
            lock.lock();
        }

        uint64 const dropped = _dropped.load();
        auto const now = std::chrono::steady_clock::now();
        if (dropped != _reportedDrops && now - lastReport >= std::chrono::seconds(10))
        {
            uint64 const newlyDropped = dropped - _reportedDrops;
            _reportedDrops = dropped;
            lastReport = now;
            lock.unlock();
            LOG_WARN("server.logging", "The DB log appender dropped {} row(s) because the login database was closed, busy or full", newlyDropped);
            lock.lock();
        }
    }
}
