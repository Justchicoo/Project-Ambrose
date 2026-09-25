/*
 * Project Ambrose by Imjustchico
 * Reads and writes the settings and setting_audit tables through one store over whichever pool an app owns: a change sets or removes its row and writes its audit row in one transaction, so a value is never persisted without its audit row nor audited without being persisted, and a key's history reads its newest audit rows first.
 */

#include "DatabaseSettingStore.h"
#include "DatabaseEnv.h"

#include <fmt/format.h>

namespace
{
    template<typename ConnectionType>
    class DatabaseSettingStore final : public SettingStore
    {
    public:
        using Pool = DatabaseWorkerPool<ConnectionType>;
        using Id = typename ConnectionType::Statements;

        struct Statements
        {
            Id Select;
            Id Replace;
            Id Remove;
            Id Audit;
            Id History;
        };

        DatabaseSettingStore(Pool& pool, Statements statements) : _pool(pool), _statements(statements)
        {
        }

        bool Load(std::map<std::string, std::string, std::less<>>& values, std::string& error) override
        {
            auto const statement = _pool.GetPreparedStatement(_statements.Select);
            PreparedQueryResult result;
            if (!statement || !_pool.TryQuery(*statement, result))
            {
                error = fmt::format("the {} database could not read its settings table; the server log names why", _pool.GetName());
                return false;
            }
            if (!result)
                return true;
            do
            {
                values[(*result)[0].template Get<std::string>()] = (*result)[1].template Get<std::string>();
            } while (result->NextRow());
            return true;
        }

        bool Write(SettingWrite const& write, std::string& error) override
        {
            auto transaction = _pool.BeginTransaction();
            if (write.Persisted)
            {
                auto statement = _pool.GetPreparedStatement(_statements.Replace);
                if (!statement)
                    return Refuse(error);
                statement->SetData(0, write.Key);
                statement->SetData(1, *write.Persisted);
                statement->SetData(2, write.Author.Who);
                statement->SetData(3, static_cast<uint64>(write.EpochSeconds));
                transaction->Append(std::move(statement));
            }
            else
            {
                auto statement = _pool.GetPreparedStatement(_statements.Remove);
                if (!statement)
                    return Refuse(error);
                statement->SetData(0, write.Key);
                transaction->Append(std::move(statement));
            }
            auto audit = _pool.GetPreparedStatement(_statements.Audit);
            if (!audit)
                return Refuse(error);
            audit->SetData(0, write.Key);
            audit->SetData(1, write.OldValue);
            audit->SetData(2, write.NewValue);
            audit->SetData(3, write.Author.Who);
            audit->SetData(4, write.Author.AccountId);
            audit->SetData(5, write.Author.Source);
            audit->SetData(6, write.Reason);
            audit->SetData(7, static_cast<uint64>(write.EpochSeconds));
            transaction->Append(std::move(audit));
            if (!_pool.DirectCommitTransaction(transaction))
                return Refuse(error);
            return true;
        }

        bool History(std::string const& key, std::size_t limit, std::vector<SettingAuditEntry>& entries, std::string& error) override
        {
            auto statement = _pool.GetPreparedStatement(_statements.History);
            if (!statement)
                return Refuse(error);
            statement->SetData(0, key);
            statement->SetData(1, static_cast<uint32>(limit));
            PreparedQueryResult result;
            if (!_pool.TryQuery(*statement, result))
                return Refuse(error);
            if (!result)
                return true;
            do
            {
                PreparedResultSet const& row = *result;
                SettingAuditEntry entry;
                entry.Id = row[0].template Get<uint64>();
                entry.Key = row[1].template Get<std::string>();
                entry.OldValue = row[2].template Get<std::string>();
                entry.NewValue = row[3].template Get<std::string>();
                entry.Who = row[4].template Get<std::string>();
                entry.AccountId = row[5].template Get<uint64>();
                entry.Source = row[6].template Get<std::string>();
                entry.Reason = row[7].template Get<std::string>();
                entry.EpochSeconds = static_cast<int64>(row[8].template Get<uint64>());
                entries.push_back(std::move(entry));
            } while (result->NextRow());
            return true;
        }

    private:
        bool Refuse(std::string& error) const
        {
            error = fmt::format("the {} database refused the change; the server log names why", _pool.GetName());
            return false;
        }

        Pool& _pool;
        Statements _statements;
    };
}

std::shared_ptr<SettingStore> SettingStores::ForCharacters()
{
    return std::make_shared<DatabaseSettingStore<CharacterDatabaseConnection>>(CharacterDatabase,
        DatabaseSettingStore<CharacterDatabaseConnection>::Statements{ CHAR_SEL_SETTINGS, CHAR_REP_SETTING, CHAR_DEL_SETTING, CHAR_INS_SETTING_AUDIT, CHAR_SEL_SETTING_AUDIT });
}

std::shared_ptr<SettingStore> SettingStores::ForLogin()
{
    return std::make_shared<DatabaseSettingStore<LoginDatabaseConnection>>(LoginDatabase,
        DatabaseSettingStore<LoginDatabaseConnection>::Statements{ LOGIN_SEL_SETTINGS, LOGIN_REP_SETTING, LOGIN_DEL_SETTING, LOGIN_INS_SETTING_AUDIT, LOGIN_SEL_SETTING_AUDIT });
}
