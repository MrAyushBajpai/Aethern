#include "AuthManager.hpp"
#include "../storage/Storage.hpp"   // Storage::loadUsers / saveUsers
#include <sodium.h>
#include <stdexcept>

// Constructor -> load users immediately
AuthManager::AuthManager(const std::string& userFile)
    : userFilePath(userFile), logged_in_user(nullptr)
{
    loadUsers();
}

// Load users from disk into the private vector
void AuthManager::loadUsers() {
    std::vector<User> loaded;
    // Storage::loadUsers returns false if file not found - that's fine (empty list)
    Storage::loadUsers(loaded, userFilePath);
    users = std::move(loaded);
}

// Save users to disk
void AuthManager::saveUsers() {
    Storage::saveUsers(users, userFilePath);
}

// Public save wrapper
void AuthManager::save() {
    saveUsers();
}

// Signup: add user if username not used. Hash the password and persist.
bool AuthManager::signup(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) return false;

    // check for existing username
    for (const auto& u : users) {
        if (u.username == username) return false;
    }

    // Hash password (may throw on error)
    std::string hashed = hashPassword(password);

    // Add user and persist
    users.emplace_back(username, hashed);
    saveUsers();
    return true;
}

// Login: verify username + password, set logged_in_user pointer
bool AuthManager::login(const std::string& username, const std::string& password) {
    for (auto& u : users) {
        if (u.username == username) {
            if (verifyPassword(password, u.password_hash)) {
                logged_in_user = &u;
                return true;
            }
            return false; // wrong password
        }
    }
    return false; // username not found
}

User* AuthManager::getCurrentUser() {
    return logged_in_user;
}

void AuthManager::logout() {
    logged_in_user = nullptr;
}

// ---------------------
// Password hashing + verification
// ---------------------

// Create a password hash string using libsodium's crypto_pwhash_str (Argon2id)
std::string AuthManager::hashPassword(const std::string& password) {
    // Ensure libsodium is initialized prior to calling this.
    // crypto_pwhash_STRBYTES is defined by libsodium and includes salt, params, etc.
    char out[crypto_pwhash_STRBYTES];

    // Use interactive limits for reasonably fast hashing on typical machines.
    // For higher security, you can use crypto_pwhash_OPSLIMIT_MODERATE / MEMLIMIT_MODERATE etc.
    if (crypto_pwhash_str(
        out,
        password.c_str(),
        static_cast<unsigned long long>(password.size()),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        // out of memory / cannot allocate
        throw std::runtime_error("crypto_pwhash_str failed (out of memory)");
    }

    return std::string(out);
}

// Verify password against stored hash
bool AuthManager::verifyPassword(const std::string& password, const std::string& hash) {
    if (hash.empty()) return false;

    // crypto_pwhash_str_verify returns 0 on success
    if (crypto_pwhash_str_verify(hash.c_str(),
        password.c_str(),
        static_cast<unsigned long long>(password.size())) == 0)
    {
        return true;
    }
    return false;
}
