#include "User.hpp"

User::User(const std::string& user, const std::string& hash)
    : username(user), password_hash(hash)
{
    created_at = std::time(nullptr);
}
