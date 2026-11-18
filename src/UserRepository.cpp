#include "UserRepository.h"
using namespace std;

static const char* BCRYPT_COST = "10";

static User readUser(const Result& r, int row)
{
    return {r.integer(row, "id"), r.text(row, "full_name"), r.text(row, "email")};
}

UserRepository::UserRepository(Database& db) : db_(db)
{
}

optional<User> UserRepository::create(const string& name, const string& email, const string& password)
{
    Result r = db_.query(R"(
        INSERT INTO users (full_name, email, password_hash)
        VALUES ($1, $2, crypt($3, gen_salt('bf', $4::int)))
        ON CONFLICT ((lower(email))) DO NOTHING
        RETURNING id, full_name, email)",
        {name, email, password, string(BCRYPT_COST)});
    if (r.rows() == 0)
    {
        return nullopt;
    }
    return readUser(r, 0);
}

optional<User> UserRepository::verifyPassword(const string& email, const string& password)
{
    Result r = db_.query(R"(
        SELECT id, full_name, email, password_hash = crypt($2, password_hash) AS ok
          FROM users WHERE lower(email) = lower($1))",
        {email, password});
    if (r.rows() == 0)
    {
        db_.query("SELECT crypt($1, gen_salt('bf', $2::int))", {password, string(BCRYPT_COST)});
        return nullopt;
    }
    if (!r.boolean(0, "ok"))
    {
        return nullopt;
    }
    return readUser(r, 0);
}

string UserRepository::createSession(int userId, int days)
{
    Result r = db_.query(R"(
        WITH token AS (SELECT encode(gen_random_bytes(32), 'hex') AS value),
             saved AS (
                INSERT INTO sessions (token_hash, user_id, expires_at)
                SELECT digest(value, 'sha256'), $1, now() + make_interval(days => $2) FROM token)
        SELECT value FROM token)",
        {toParam(userId), toParam(days)});
    return r.text(0, "value");
}

optional<User> UserRepository::userForSession(const string& token)
{
    Result r = db_.query(R"(
        SELECT u.id, u.full_name, u.email
          FROM sessions s JOIN users u ON u.id = s.user_id
         WHERE s.token_hash = digest($1, 'sha256') AND s.expires_at > now())",
        {token});
    if (r.rows() == 0)
    {
        return nullopt;
    }
    return readUser(r, 0);
}

void UserRepository::deleteSession(const string& token)
{
    db_.query("DELETE FROM sessions WHERE token_hash = digest($1, 'sha256')", {token});
}

long UserRepository::deleteExpiredSessions()
{
    return db_.query("DELETE FROM sessions WHERE expires_at < now()").affected();
}

optional<User> UserRepository::findById(int userId)
{
    Result r = db_.query("SELECT id, full_name, email FROM users WHERE id = $1", {toParam(userId)});
    if (r.rows() == 0)
    {
        return nullopt;
    }
    return readUser(r, 0);
}

void UserRepository::updateName(int userId, const string& name)
{
    db_.query("UPDATE users SET full_name = $2, updated_at = now() WHERE id = $1", {toParam(userId), name});
}

bool UserRepository::changePassword(int userId, const string& currentPassword, const string& newPassword)
{
    bool changed = false;
    db_.transaction([&](Transaction& tx) {
        Result r = tx.query(R"(
            UPDATE users SET password_hash = crypt($3, gen_salt('bf', $4::int)), updated_at = now()
             WHERE id = $1 AND password_hash = crypt($2, password_hash)
            RETURNING id)",
            {toParam(userId), currentPassword, newPassword, string(BCRYPT_COST)});
        changed = r.rows() > 0;
        if (changed)
        {
            tx.query("DELETE FROM sessions WHERE user_id = $1", {toParam(userId)});
        }
    });
    return changed;
}

bool UserRepository::deleteAccount(int userId, const string& password)
{
    Result r = db_.query("DELETE FROM users WHERE id = $1 AND password_hash = crypt($2, password_hash) RETURNING id",
                         {toParam(userId), password});
    return r.rows() > 0;
}
