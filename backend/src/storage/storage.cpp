#include "Storage.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <sodium.h>

static const char MAGIC_HDR[] = "SRDATA1\n"; // 8 bytes

// -------------------- USERS --------------------

bool Storage::saveUsers(const std::vector<User>& users, const std::string& filename) {
    std::ofstream out(filename, std::ios::trunc);
    if (!out) return false;

    for (const auto& u : users) {
        out << u.username << "\n";
        out << u.password_hash << "\n";
        out << u.enc_salt << "\n";
        out << u.created_at << "\n";
        out << "---\n";
    }
    return true;
}

bool Storage::loadUsers(std::vector<User>& users, const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        // file not present => treat as no users
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
        std::getline(in, sep); // eat newline after timestamp
        std::getline(in, sep); // read --- line

        users.push_back(u);
    }
    return true;
}

// -------------------- ITEMS (encrypted) --------------------

// plaintext serialization: same textual format as before
static std::string serializeItemsPlain(const std::vector<Item>& items) {
    std::ostringstream oss;
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
        std::getline(iss, sep); // newline after next_review
        std::getline(iss, sep); // '---'
        items.push_back(it);
    }
    return true;
}

// Save encrypted items: header + nonce + ciphertext
bool Storage::saveItems(const std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key) {
    if (key.size() != crypto_secretbox_KEYBYTES) return false;

    // serialize to plaintext
    std::string plain = serializeItemsPlain(items);
    const unsigned char* plain_buf = reinterpret_cast<const unsigned char*>(plain.data());
    unsigned long long plain_len = plain.size();

    // allocate ciphertext: plain_len + crypto_secretbox_MACBYTES
    std::vector<unsigned char> ciphertext(plain_len + crypto_secretbox_MACBYTES);

    // nonce
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    if (crypto_secretbox_easy(ciphertext.data(), plain_buf, plain_len, nonce, key.data()) != 0) {
        return false;
    }

    // write to file
    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out.write(MAGIC_HDR, sizeof(MAGIC_HDR) - 1); // exclude implicit null
    out.write(reinterpret_cast<const char*>(nonce), sizeof(nonce));

    // optionally write ciphertext length (not strictly necessary; will read file size to determine)
    // write ciphertext
    out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    out.close();

    return true;
}

bool Storage::loadItems(std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key) {
    items.clear();
    if (key.size() != crypto_secretbox_KEYBYTES) return false;

    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        // no file: treat as empty
        return true;
    }

    // read header
    char hdr[sizeof(MAGIC_HDR) - 1];
    in.read(hdr, sizeof(hdr));
    if (in.gcount() != (std::streamsize)(sizeof(hdr))) {
        return false;
    }
    if (std::strncmp(hdr, MAGIC_HDR, sizeof(hdr)) != 0) {
        // not our file format
        return false;
    }

    // read nonce
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    in.read(reinterpret_cast<char*>(nonce), sizeof(nonce));
    if (in.gcount() != (std::streamsize)sizeof(nonce)) return false;

    // read remaining ciphertext
    std::vector<unsigned char> ciphertext((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (ciphertext.size() < crypto_secretbox_MACBYTES) return false;

    // prepare plaintext buffer
    std::vector<unsigned char> plain(ciphertext.size() - crypto_secretbox_MACBYTES);

    if (crypto_secretbox_open_easy(plain.data(), ciphertext.data(), ciphertext.size(), nonce, key.data()) != 0) {
        // decryption failed (wrong key or tampering)
        return false;
    }

    // convert plaintext to string and parse
    std::string plain_str(reinterpret_cast<char*>(plain.data()), plain.size());
    parsePlainToItems(plain_str, items);
    return true;
}
