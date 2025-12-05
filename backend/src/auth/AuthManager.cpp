#include "AuthManager.hpp"
#include "../storage/Storage.hpp"
#include <sodium.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>

// constants for key derivation / pwhash
static constexpr std::size_t ENC_KEY_BYTES = crypto_secretbox_KEYBYTES; // 32
static constexpr std::size_t SALT_BYTES = crypto_pwhash_SALTBYTES;      // recommended salt size

AuthManager::AuthManager(const std::string& userFile)
    : userFilePath(userFile), logged_in_user(nullptr)
{
    // libsodium must be initialized by caller (main)
    loadUsers();
}

void AuthManager::loadUsers() {
    std::vector<User> loaded;
    Storage::loadUsers(loaded, userFilePath);
    users = std::move(loaded);
}

void AuthManager::saveUsers() {
    Storage::saveUsers(users, userFilePath);
}

void AuthManager::save() {
    saveUsers();
}

std::string AuthManager::hashPassword(const std::string& password) {
    char out[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
        out,
        password.c_str(),
        static_cast<unsigned long long>(password.size()),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        throw std::runtime_error("crypto_pwhash_str failed (out of memory)");
    }

    return std::string(out);
}

bool AuthManager::verifyPassword(const std::string& password, const std::string& hash) {
    if (hash.empty()) return false;

    if (crypto_pwhash_str_verify(hash.c_str(),
        password.c_str(),
        static_cast<unsigned long long>(password.size())) == 0)
    {
        return true;
    }
    return false;
}

// Helper: hex-encode salt bytes to string
static std::string saltToHex(const unsigned char* salt, size_t len) {
    // hex length = 2 * len + 1 (null)
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
        /*ignore*/ nullptr, &bin_len, /*hex_end*/ nullptr) != 0) {
        return false;
    }
    if (bin_len != SALT_BYTES) return false;
    return true;
}

// derive session key using crypto_pwhash with the user's stored salt
bool AuthManager::deriveSessionKey(const std::string& password, const std::string& salt_hex) {
    if (salt_hex.empty()) return false;

    std::vector<unsigned char> salt;
    if (!hexToSalt(salt_hex, salt)) return false;

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
        // out of memory
        session_key.clear();
        return false;
    }
    return true;
}

bool AuthManager::signup(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) return false;

    for (const auto& u : users) {
        if (u.username == username) return false; // already exists
    }

    // hash password
    std::string hashed = hashPassword(password);

    // generate random salt for encryption key derivation
    unsigned char salt[SALT_BYTES];
    randombytes_buf(salt, SALT_BYTES);
    std::string salt_hex = saltToHex(salt, SALT_BYTES);

    // create user and persist
    users.emplace_back(username, hashed, salt_hex);
    saveUsers();
    return true;
}

bool AuthManager::login(const std::string& username, const std::string& password) {
    for (auto& u : users) {
        if (u.username == username) {
            if (verifyPassword(password, u.password_hash)) {
                // derive session key for user's salt
                if (!deriveSessionKey(password, u.enc_salt)) {
                    return false;
                }
                logged_in_user = &u;
                return true;
            }
            return false; // wrong password
        }
    }
    return false; // no user
}

User* AuthManager::getCurrentUser() {
    return logged_in_user;
}

void AuthManager::logout() {
    logged_in_user = nullptr;
    // Clear session key from memory
    if (!session_key.empty()) {
        sodium_memzero(session_key.data(), session_key.size());
        session_key.clear();
    }
}

const std::vector<unsigned char>& AuthManager::getSessionKey() const {
    return session_key;
}
