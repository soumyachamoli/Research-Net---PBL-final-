#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using Param = std::optional<std::string>;
using Params = std::vector<Param>;

class DatabaseError : public std::runtime_error
{
public:
    DatabaseError(const std::string& message, const std::string& sqlState)
        : std::runtime_error(message), sqlState_(sqlState) {}
    const std::string& sqlState() const { return sqlState_; }
private:
    std::string sqlState_;
};

class Result
{
public:
    explicit Result(PGresult* result);
    int rows() const;
    int column(const std::string& name) const;
    bool isNull(int row, const std::string& column) const;
    std::string text(int row, const std::string& column) const;
    int integer(int row, const std::string& column) const;
    std::optional<double> number(int row, const std::string& column) const;
    bool boolean(int row, const std::string& column) const;
private:
    std::shared_ptr<PGresult> result_;
};

class Transaction
{
public:
    Result query(const std::string& sql, const Params& params = {});

private:
    friend class Database;
    explicit Transaction(PGconn* conn) : conn_(conn) {}
    PGconn* conn_;
};

class Database
{
public:
    Database(const std::string& connectionInfo, int poolSize);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Result query(const std::string& sql, const Params& params = {});
    void transaction(const std::function<void(Transaction&)>& work);

private:
    std::string connectionInfo_;
    std::vector<PGconn*> idle_;
    std::vector<PGconn*> all_;
    std::mutex mutex_;
    std::condition_variable available_;

    PGconn* acquire();
    void release(PGconn* conn);
};

Param toParam(int value);
Param toParam(double value);
Param toParam(const std::string& value);
Param toArrayParam(const std::vector<int>& values);
Param toArrayParam(const std::vector<double>& values);

#endif
