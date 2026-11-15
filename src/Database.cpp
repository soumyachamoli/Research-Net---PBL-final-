#include "Database.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
using namespace std;


Result::Result(PGresult* result) : result_(result, PQclear)
{
}

int Result::rows() const
{
    return PQntuples(result_.get());
}

int Result::column(const string& name) const
{
    int index = PQfnumber(result_.get(), name.c_str());
    if (index < 0)
    {
        throw logic_error("query result has no column " + name);
    }
    return index;
}

bool Result::isNull(int row, const string& name) const
{
    return PQgetisnull(result_.get(), row, column(name)) == 1;
}

string Result::text(int row, const string& name) const
{
    return PQgetvalue(result_.get(), row, column(name));
}

int Result::integer(int row, const string& name) const
{
    return isNull(row, name) ? 0 : atoi(text(row, name).c_str());
}

optional<double> Result::number(int row, const string& name) const
{
    if (isNull(row, name))
    {
        return nullopt;
    }
    return strtod(text(row, name).c_str(), nullptr);
}

bool Result::boolean(int row, const string& name) const
{
    return text(row, name) == "t";
}


Database::Database(const string& connectionInfo, int poolSize) : connectionInfo_(connectionInfo)
{
    for (int i = 0; i < poolSize; i++)
    {
        PGconn* conn = PQconnectdb(connectionInfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK)
        {
            string message = PQerrorMessage(conn);
            PQfinish(conn);
            for (PGconn* open : all_)
            {
                PQfinish(open);
            }
            throw DatabaseError("Connection to PostgreSQL failed: " + message, "");
        }
        all_.push_back(conn);
        idle_.push_back(conn);
    }
}

Database::~Database()
{
    for (PGconn* conn : all_)
    {
        PQfinish(conn);
    }
}

PGconn* Database::acquire()
{
    unique_lock<mutex> lock(mutex_);
    available_.wait(lock, [this] { return !idle_.empty(); });
    PGconn* conn = idle_.back();
    idle_.pop_back();
    lock.unlock();

    if (PQstatus(conn) != CONNECTION_OK)
    {
        PQreset(conn);
    }
    return conn;
}

void Database::release(PGconn* conn)
{
    {
        lock_guard<mutex> lock(mutex_);
        idle_.push_back(conn);
    }
    available_.notify_one();
}

Result Database::query(const string& sql, const Params& params)
{
    vector<const char*> values;
    for (const Param& p : params)
    {
        values.push_back(p ? p->c_str() : nullptr);
    }

    PGconn* conn = acquire();
    PGresult* raw = PQexecParams(conn, sql.c_str(), (int)values.size(), nullptr,
                                 values.empty() ? nullptr : values.data(), nullptr, nullptr, 0);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        PQclear(raw);
        PQreset(conn);
        raw = PQexecParams(conn, sql.c_str(), (int)values.size(), nullptr,
                           values.empty() ? nullptr : values.data(), nullptr, nullptr, 0);
    }
    Result result(raw);
    ExecStatusType status = PQresultStatus(raw);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
    {
        const char* state = PQresultErrorField(raw, PG_DIAG_SQLSTATE);
        string message = PQresultErrorMessage(raw);
        if (message.empty())
        {
            message = PQerrorMessage(conn);
        }
        release(conn);
        throw DatabaseError(message, state ? state : "");
    }
    release(conn);
    return result;
}


Param toParam(int value)
{
    return to_string(value);
}

Param toParam(double value)
{
    ostringstream text;
    text.precision(10);
    text << value;
    return text.str();
}

Param toParam(const string& value)
{
    return value;
}

Param toArrayParam(const vector<int>& values)
{
    string text = "{";
    for (size_t i = 0; i < values.size(); i++)
    {
        text += (i > 0 ? "," : "") + to_string(values[i]);
    }
    return text + "}";
}

Param toArrayParam(const vector<double>& values)
{
    ostringstream text;
    text.precision(10);
    text << "{";
    for (size_t i = 0; i < values.size(); i++)
    {
        text << (i > 0 ? "," : "") << values[i];
    }
    text << "}";
    return text.str();
}
