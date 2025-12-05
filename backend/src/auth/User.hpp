#pragma once
#include <string>
#include <ctime>

class User {
public:
    User() = default;
    User(const std::string& user, const std::string& hash, const std::string& salt_hex = "");

    std::string username;
    std::string password_hash; // Argon2id hash (crypto_pwhash_str)
    std::string enc_salt;      // hex-encoded salt for key derivation
    std::time_t created_at = 0;
};
