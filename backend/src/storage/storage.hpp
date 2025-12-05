#pragma once
#include <vector>
#include <string>
#include "../core/Item.hpp"
#include "../auth/User.hpp"

// Storage handles user file IO and per-user encrypted item files.
//
// For users: text-based safe lines (username, hash, salt_hex, created_at, ---)
// For items: encrypted binary format:
//   Header: 8 bytes ASCII "SRDATA1\n" (magic + version)
//   Nonce: crypto_secretbox_NONCEBYTES
//   Ciphertext: remaining bytes
//
// saveItems/loadItems require a derived key (binary vector). If key is empty, returns false.

class Storage {
public:
    // USERS (text)
    static bool saveUsers(const std::vector<User>& users, const std::string& filename);
    static bool loadUsers(std::vector<User>& users, const std::string& filename);

    // ITEMS (encrypted)
    static bool saveItems(const std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key);
    static bool loadItems(std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key);
};
