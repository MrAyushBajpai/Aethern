#include "User.hpp"

User::User(const std::string& user, const std::string& hash, const std::string& salt_hex)
    : username(user), password_hash(hash), enc_salt(salt_hex)
{
    created_at = std::time(nullptr);
}
