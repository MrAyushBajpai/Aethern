#include "AuthManager.hpp"
#include "../storage/Storage.hpp"
#include <sodium.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include <spdlog/spdlog.h>

// constants for key derivation / pwhash
static constexpr std::size_t ENC_KEY_BYTES = crypto_secretbox_KEYBYTES; // 32
static constexpr std::size_t SALT_BYTES = crypto_pwhash_SALTBYTES;      // recommended salt size

AuthManager::AuthManager(const std::string& userFile)
    : userFilePath(userFile), logged_in_user(nullptr)
{
    spdlog::info("AuthManager initialized with user file '{}'", userFilePath);
    loadUsers();
}

void AuthManager::loadUsers() {
    spdlog::debug("Loading users from '{}'", userFilePath);

    std::vector<User> loaded;
    Storage::loadUsers(loaded, userFilePath);

    spdlog::info("Loaded {} user entries", loaded.size());
    users = std::move(loaded);
}

void AuthManager::saveUsers() {
    spdlog::debug("Saving {} user entries to '{}'", users.size(), userFilePath);
    Storage::saveUsers(users, userFilePath);
    spdlog::info("User data saved successfully");
}

void AuthManager::save() {
    spdlog::debug("save() called");
    saveUsers();
}

std::string AuthManager::hashPassword(const std::string& password) {
    spdlog::debug("Hashing password (not logging the password)");

    char out[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
        out,
        password.c_str(),
        static_cast<unsigned long long>(password.size()),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        spdlog::error("crypto_pwhash_str failed (likely out of memory)");
        throw std::runtime_error("crypto_pwhash_str failed (out of memory)");
    }

    spdlog::debug("Password hashed successfully");
    return std::string(out);
}

bool AuthManager::verifyPassword(const std::string& password, const std::string& hash) {
    spdlog::debug("Verifying password (not logging password or hash)");

    if (hash.empty()) {
        spdlog::warn("verifyPassword() called with empty hash");
        return false;
    }

    if (crypto_pwhash_str_verify(hash.c_str(),
        password.c_str(),
        static_cast<unsigned long long>(password.size())) == 0)
    {
        spdlog::debug("Password verified successfully");
        return true;
    }

    spdlog::warn("Password verification failed");
    return false;
}

// Helper: hex-encode salt bytes to string
static std::string saltToHex(const unsigned char* salt, size_t len) {
    char* hex = (char*)sodium_malloc(2 * len + 1);
    sodium_bin2hex(hex, 2 * len + 1, salt, len);
    std::string s(hex);
    sodium_free(hex);
    return s;
}

// Helper: hex string to bytes (frees vector on failure)
static bool hexToSalt(const std::string& hex, std::vector<unsigned char>& out) {
    out.resize(SALT_BYTES);
    size_t bin_len = 0;

    if (sodium_hex2bin(out.data(), out.size(),
        hex.c_str(), hex.size(),
        nullptr, &bin_len, nullptr) != 0)
    {
        spdlog::error("Failed to convert hex salt to binary");
        return false;
    }

    if (bin_len != SALT_BYTES) {
        spdlog::error("Salt length mismatch while decoding");
        return false;
    }

    return true;
}

bool AuthManager::deriveSessionKey(const std::string& password, const std::string& salt_hex) {
    spdlog::debug("Deriving session key (not logging password or salt)");

    if (salt_hex.empty()) {
        spdlog::error("Cannot derive session key: salt is empty");
        return false;
    }

    std::vector<unsigned char> salt;
    if (!hexToSalt(salt_hex, salt)) {
        spdlog::error("Failed to decode salt for session key derivation");
        return false;
    }

    session_key.assign(ENC_KEY_BYTES, 0);

    if (crypto_pwhash(session_key.data(),
        ENC_KEY_BYTES,
        password.c_str(),
        static_cast<unsigned long long>(password.size()),
        salt.data(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT) != 0)
    {
        spdlog::error("crypto_pwhash failed during session key derivation");
        session_key.clear();
        return false;
    }

    spdlog::debug("Session key derived successfully");
    return true;
}

bool AuthManager::signup(const std::string& username, const std::string& password) {
    spdlog::info("Attempting signup for username '{}'", username);

    if (username.empty() || password.empty()) {
        spdlog::warn("Signup failed: empty username or password");
        return false;
    }

    for (const auto& u : users) {
        if (u.username == username) {
            spdlog::warn("Signup failed: username '{}' already exists", username);
            return false;
        }
    }

    spdlog::debug("Hashing password for new user '{}'", username);
    std::string hashed = hashPassword(password);

    unsigned char salt[SALT_BYTES];
    randombytes_buf(salt, SALT_BYTES);
    std::string salt_hex = saltToHex(salt, SALT_BYTES);

    users.emplace_back(username, hashed, salt_hex);
    saveUsers();

    spdlog::info("Signup successful for username '{}'", username);
    return true;
}

bool AuthManager::login(const std::string& username, const std::string& password) {
    spdlog::info("Login attempt for username '{}'", username);

    for (auto& u : users) {
        if (u.username == username) {

            if (verifyPassword(password, u.password_hash)) {
                spdlog::debug("Password verification successful for '{}'", username);

                if (!deriveSessionKey(password, u.enc_salt)) {
                    spdlog::error("Failed to derive session key for '{}'", username);
                    return false;
                }

                logged_in_user = &u;
                spdlog::info("User '{}' logged in successfully", username);
                return true;
            }

            spdlog::warn("Login failed: incorrect password for '{}'", username);
            return false;
        }
    }

    spdlog::warn("Login failed: username '{}' not found", username);
    return false;
}

User* AuthManager::getCurrentUser() {
    if (logged_in_user)
        spdlog::debug("getCurrentUser(): a user is logged in");
    else
        spdlog::debug("getCurrentUser(): no user logged in");

    return logged_in_user;
}

void AuthManager::logout() {
    if (logged_in_user)
        spdlog::info("User '{}' logging out", logged_in_user->username);
    else
        spdlog::debug("logout() called but no user was logged in");

    logged_in_user = nullptr;

    if (!session_key.empty()) {
        spdlog::debug("Clearing session key from memory");
        sodium_memzero(session_key.data(), session_key.size());
        session_key.clear();
    }
}

const std::vector<unsigned char>& AuthManager::getSessionKey() const {
    spdlog::debug("Session key requested (not logging key contents)");
    return session_key;
}
