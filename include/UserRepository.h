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

// Accounts and login sessions, stored in PostgreSQL.
//
// Passwords: hashed with bcrypt by PostgreSQL's pgcrypto extension
//   crypt(password, gen_salt('bf', 10))  ->  "$2a$10$..." (salted, slow on purpose)
// so the plain password is never stored. Login compares crypt(attempt, stored_hash).
//
// Sessions: a random 256-bit token (gen_random_bytes) is given to the browser in a cookie;
// the database only keeps its SHA-256 hash.
class UserRepository
{
public:
    explicit UserRepository(Database& db);

    // nullopt if an account with this email already exists.
    std::optional<User> create(const std::string& name, const std::string& email, const std::string& password);

    // nullopt if the email is unknown or the password is wrong.
    std::optional<User> verifyPassword(const std::string& email, const std::string& password);

    std::string createSession(int userId, int days);
    std::optional<User> userForSession(const std::string& token);
    void deleteSession(const std::string& token);

    std::optional<User> findById(int userId);
    void updateName(int userId, const std::string& name);
    bool changePassword(int userId, const std::string& currentPassword, const std::string& newPassword);

private:
    Database& db_;
};

#endif
