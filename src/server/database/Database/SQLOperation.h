/*
 * Project Ambrose by Imjustchico
 * The interface of queued database work: run on a worker's connection, or settle empty when the pool closes or cancels it first.
 */

#ifndef AMBROSE_SQLOPERATION_H
#define AMBROSE_SQLOPERATION_H

class MySQLConnection;

class SQLOperation
{
public:
    virtual ~SQLOperation() = default;

    virtual void Execute(MySQLConnection& connection) = 0;
    virtual void Cancel() = 0;
};

#endif
