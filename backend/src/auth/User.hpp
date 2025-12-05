#pragma once
#include <string>
#include <ctime>

class User {
public:
    std::string username;
    std::string password_hash; // Argon2id hash
    std::time_t created_at = 0;

    User() = default;
    User(const std::string& user, const std::string& hash);
};
