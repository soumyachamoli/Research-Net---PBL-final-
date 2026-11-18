#ifndef USERREPOSITORY_H
#define USERREPOSITORY_H

#include "Database.h"
#include <optional>
#include <string>

struct User
{
    int id = 0;
    std::string name;
    std::string email;
};

class UserRepository
{
public:
    explicit UserRepository(Database& db);

    std::optional<User> create(const std::string& name, const std::string& email, const std::string& password);

    std::optional<User> verifyPassword(const std::string& email, const std::string& password);

    std::string createSession(int userId, int days);
    std::optional<User> userForSession(const std::string& token);
    void deleteSession(const std::string& token);
    long deleteExpiredSessions();

    std::optional<User> findById(int userId);
    void updateName(int userId, const std::string& name);
    bool changePassword(int userId, const std::string& currentPassword, const std::string& newPassword);

private:
    Database& db_;
};

#endif
