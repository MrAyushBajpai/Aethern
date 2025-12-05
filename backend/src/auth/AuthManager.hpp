#pragma once

#include <string>
#include <vector>
#include "User.hpp"

class AuthManager {
public:
    // Construct with an optional filepath for users storage
    explicit AuthManager(const std::string& userFile = "users.txt");

    // Authentication API for callers (main.cpp etc.)
    bool signup(const std::string& username, const std::string& password);
    bool login(const std::string& username, const std::string& password);

    // Returns pointer to the currently logged-in user, or nullptr
    User* getCurrentUser();
    void logout();

    // Persist users to disk (calls Storage)
    void save();

private:
    std::vector<User> users;         // owned user list (not exposed)
    User* logged_in_user = nullptr;
    std::string userFilePath;

    // Internal storage helpers
    void loadUsers();   // populates `users` from file
    void saveUsers();   // writes `users` to file

    // Password hashing / verification (libsodium - Argon2id via crypto_pwhash)
    std::string hashPassword(const std::string& password);
    bool verifyPassword(const std::string& password, const std::string& hash);
};
