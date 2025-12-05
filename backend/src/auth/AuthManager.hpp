#pragma once

#include <string>
#include <vector>
#include "User.hpp"

// Forward-declare libsodium types not required
class AuthManager {
public:
    explicit AuthManager(const std::string& userFile = "users.txt");

    bool signup(const std::string& username, const std::string& password);
    bool login(const std::string& username, const std::string& password);

    User* getCurrentUser();
    void logout();

    // Persist users to disk
    void save();

    // Return the in-memory session key (derived from password) for the currently logged-in user.
    // If no user is logged in, returns an empty vector.
    const std::vector<unsigned char>& getSessionKey() const;

private:
    std::vector<User> users;         // private user list
    User* logged_in_user = nullptr;
    std::string userFilePath;

    std::vector<unsigned char> session_key; // holds derived key for current session

    // Storage helpers
    void loadUsers();
    void saveUsers();

    // Password hashing / verification (libsodium)
    std::string hashPassword(const std::string& password);
    bool verifyPassword(const std::string& password, const std::string& hash);

    // Key derivation: derive session_key from password + user's salt (stored as hex)
    // returns true on success, false on failure
    bool deriveSessionKey(const std::string& password, const std::string& salt_hex);
};
