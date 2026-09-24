/*
 * Project Ambrose by Imjustchico
 * Forward declarations and pointer aliases for the database layer, so headers can name results and connections without the connector.
 */

#ifndef AMBROSE_DATABASEENVFWD_H
#define AMBROSE_DATABASEENVFWD_H

#include <memory>

class Field;
class ResultSet;
class PreparedResultSet;
class PreparedStatementBase;
class MySQLConnection;
struct MySQLConnectionInfo;

using QueryResult = std::shared_ptr<ResultSet>;
using PreparedQueryResult = std::shared_ptr<PreparedResultSet>;

#endif
