#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// A query parameter: text, or std::nullopt for SQL NULL. Values are always sent separately
// from the SQL text ($1, $2 ...), so user input can never change the query (no SQL injection).
using Param = std::optional<std::string>;
using Params = std::vector<Param>;

class DatabaseError : public std::runtime_error
{
public:
    DatabaseError(const std::string& message, const std::string& sqlState)
        : std::runtime_error(message), sqlState_(sqlState) {}
    const std::string& sqlState() const { return sqlState_; }   // "23505" = unique violation
private:
    std::string sqlState_;
};

// The rows returned by a query. Frees the libpq result automatically.
class Result
{
public:
    explicit Result(PGresult* result);
    int rows() const;
    int column(const std::string& name) const;    // throws if the column does not exist
    bool isNull(int row, const std::string& column) const;
    std::string text(int row, const std::string& column) const;      // "" for NULL
    int integer(int row, const std::string& column) const;          // 0 for NULL
    std::optional<double> number(int row, const std::string& column) const;
    bool boolean(int row, const std::string& column) const;
private:
    std::shared_ptr<PGresult> result_;
};

// A small pool of PostgreSQL connections, so several web requests can use the
// database at the same time. Each query borrows a connection and gives it back.
class Database
{
public:
    Database(const std::string& connectionInfo, int poolSize);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Result query(const std::string& sql, const Params& params = {});

private:
    std::string connectionInfo_;
    std::vector<PGconn*> idle_;
    std::vector<PGconn*> all_;
    std::mutex mutex_;
    std::condition_variable available_;

    PGconn* acquire();
    void release(PGconn* conn);
};

// Helpers to build parameters.
Param toParam(int value);
Param toParam(double value);
Param toParam(const std::string& value);
Param toArrayParam(const std::vector<int>& values);       // {1,2,3}
Param toArrayParam(const std::vector<double>& values);

#endif
