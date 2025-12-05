#include "Storage.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <sodium.h>
#include <spdlog/spdlog.h>

static const char MAGIC_HDR[] = "SRDATA1\n"; // 8 bytes

// -------------------- USERS --------------------

bool Storage::saveUsers(const std::vector<User>& users, const std::string& filename) {
    spdlog::info("Saving {} users to '{}'", users.size(), filename);

    std::ofstream out(filename, std::ios::trunc);
    if (!out) {
        spdlog::error("Failed to open '{}' for writing user data", filename);
        return false;
    }

    for (const auto& u : users) {
        out << u.username << "\n";
        out << u.password_hash << "\n";
        out << u.enc_salt << "\n";
        out << u.created_at << "\n";
        out << "---\n";
    }

    spdlog::debug("User data written to '{}'", filename);
    return true;
}

bool Storage::loadUsers(std::vector<User>& users, const std::string& filename) {
    spdlog::info("Loading users from '{}'", filename);

    std::ifstream in(filename);
    if (!in) {
        spdlog::warn("User file '{}' not found. Treating as empty user database.", filename);
        users.clear();
        return false;
    }

    users.clear();
    while (true) {
        User u;
        if (!std::getline(in, u.username)) break;
        if (!std::getline(in, u.password_hash)) break;
        if (!std::getline(in, u.enc_salt)) break;
        if (!(in >> u.created_at)) break;

        std::string sep;
        std::getline(in, sep);
        std::getline(in, sep);

        users.push_back(u);
    }

    spdlog::info("Loaded {} users from '{}'", users.size(), filename);
    return true;
}

// -------------------- ITEMS (encrypted) --------------------

static std::string serializeItemsPlain(const std::vector<Item>& items) {
    std::ostringstream oss;
    spdlog::debug("Serializing {} items to plaintext (not logging contents)", items.size());

    for (const auto& it : items) {
        oss << it.title << "\n";
        oss << it.content << "\n";
        oss << it.interval << "\n";
        oss << it.ease_factor << "\n";
        oss << it.last_review << "\n";
        oss << it.next_review << "\n";
        oss << "---\n";
    }
    return oss.str();
}

static bool parsePlainToItems(const std::string& plain, std::vector<Item>& items) {
    spdlog::debug("Parsing plaintext into items (content not logged)");

    std::istringstream iss(plain);
    items.clear();

    while (true) {
        Item it;
        if (!std::getline(iss, it.title)) break;
        std::getline(iss, it.content);
        if (!(iss >> it.interval)) break;
        if (!(iss >> it.ease_factor)) break;
        if (!(iss >> it.last_review)) break;
        if (!(iss >> it.next_review)) break;

        std::string sep;
        std::getline(iss, sep);
        std::getline(iss, sep);

        items.push_back(it);
    }

    spdlog::debug("Parsed {} items", items.size());
    return true;
}

bool Storage::saveItems(const std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key) {
    spdlog::info("Saving {} encrypted items to '{}'", items.size(), filename);

    if (key.size() != crypto_secretbox_KEYBYTES) {
        spdlog::error("Invalid encryption key size: expected {} bytes", crypto_secretbox_KEYBYTES);
        return false;
    }

    std::string plain = serializeItemsPlain(items);
    const unsigned char* plain_buf = reinterpret_cast<const unsigned char*>(plain.data());
    unsigned long long plain_len = plain.size();

    std::vector<unsigned char> ciphertext(plain_len + crypto_secretbox_MACBYTES);

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    if (crypto_secretbox_easy(ciphertext.data(), plain_buf, plain_len, nonce, key.data()) != 0) {
        spdlog::error("Encryption failed in saveItems()");
        return false;
    }

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("Failed to open '{}' for writing encrypted items", filename);
        return false;
    }

    out.write(MAGIC_HDR, sizeof(MAGIC_HDR) - 1);
    out.write(reinterpret_cast<const char*>(nonce), sizeof(nonce));
    out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    out.close();

    spdlog::info("Encrypted items saved successfully to '{}'", filename);
    return true;
}

bool Storage::loadItems(std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key) {
    spdlog::info("Loading encrypted items from '{}'", filename);
    items.clear();

    if (key.size() != crypto_secretbox_KEYBYTES) {
        spdlog::error("Invalid key size for loadItems()");
        return false;
    }

    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        spdlog::warn("Item file '{}' not found. Treating as empty item list.", filename);
        return true;
    }

    char hdr[sizeof(MAGIC_HDR) - 1];
    in.read(hdr, sizeof(hdr));
    if (in.gcount() != (std::streamsize)sizeof(hdr)) {
        spdlog::error("Invalid file header length for '{}'", filename);
        return false;
    }

    if (std::strncmp(hdr, MAGIC_HDR, sizeof(hdr)) != 0) {
        spdlog::error("'{}' is not a valid item file (magic header mismatch)", filename);
        return false;
    }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    in.read(reinterpret_cast<char*>(nonce), sizeof(nonce));
    if (in.gcount() != (std::streamsize)sizeof(nonce)) {
        spdlog::error("Failed to read nonce from '{}'", filename);
        return false;
    }

    std::vector<unsigned char> ciphertext((std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    if (ciphertext.size() < crypto_secretbox_MACBYTES) {
        spdlog::error("Ciphertext too short in '{}'", filename);
        return false;
    }

    std::vector<unsigned char> plain(ciphertext.size() - crypto_secretbox_MACBYTES);

    if (crypto_secretbox_open_easy(plain.data(), ciphertext.data(), ciphertext.size(), nonce, key.data()) != 0) {
        spdlog::error("Decryption failed: wrong key or tampered data in '{}'", filename);
        return false;
    }

    std::string plain_str(reinterpret_cast<char*>(plain.data()), plain.size());
    parsePlainToItems(plain_str, items);

    spdlog::info("Successfully loaded {} decrypted items from '{}'", items.size(), filename);
    return true;
}
