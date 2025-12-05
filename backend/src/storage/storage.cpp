#include "Storage.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <sodium.h>
#include <spdlog/spdlog.h>

static const char MAGIC_HDR[] = "SRDATA1\n";

bool Storage::saveUsers(const std::vector<User>& users, const std::string& filename) {
    spdlog::info("Saving {} users to '{}'", users.size(), filename);
    std::ofstream out(filename, std::ios::trunc);
    if (!out) {
        spdlog::error("Failed to open '{}' for writing user data", filename);
        return false;
    }

    for (const auto& u : users) {
        out << u.username << "\n"
            << u.password_hash << "\n"
            << u.enc_salt << "\n"
            << u.created_at << "\n"
            << "---\n";
    }
    return true;
}

bool Storage::loadUsers(std::vector<User>& users, const std::string& filename) {
    spdlog::info("Loading users from '{}'", filename);
    std::ifstream in(filename);
    if (!in) {
        spdlog::warn("User file '{}' not found; treating as empty", filename);
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

    spdlog::info("Loaded {} users", users.size());
    return true;
}

static std::string serializeItemsPlain(const std::vector<Item>& items) {
    std::ostringstream oss;

    for (const auto& it : items) {
        oss << it.title << "\n"
            << it.content << "\n"
            << it.tagsAsLine() << "\n"
            << it.interval << "\n"
            << it.ease_factor << "\n"
            << it.last_review << "\n"
            << it.next_review << "\n";

        oss << it.history.size() << "\n";
        for (const auto& r : it.history) {
            oss << r.timestamp << " "
                << r.quality << " "
                << r.interval_after << "\n";
        }

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

        std::string tags_line;
        if (!std::getline(iss, tags_line)) break;

        it.tags.clear();
        std::istringstream tss(tags_line);
        std::string tag;
        while (std::getline(tss, tag, ',')) {
            while (!tag.empty() && std::isspace((unsigned char)tag.front())) tag.erase(tag.begin());
            while (!tag.empty() && std::isspace((unsigned char)tag.back())) tag.pop_back();
            if (!tag.empty()) it.tags.push_back(tag);
        }

        if (!(iss >> it.interval)) break;
        if (!(iss >> it.ease_factor)) break;
        if (!(iss >> it.last_review)) break;
        if (!(iss >> it.next_review)) break;

        size_t hist_count = 0;
        if (!(iss >> hist_count)) break;

        std::string sep;
        std::getline(iss, sep);

        it.history.clear();
        for (size_t i = 0; i < hist_count; ++i) {
            ReviewRecord r;
            if (!(iss >> r.timestamp >> r.quality >> r.interval_after)) break;
            std::getline(iss, sep);
            it.history.push_back(r);
        }

        std::getline(iss, sep);
        std::getline(iss, sep);

        items.push_back(it);
    }

    return true;
}

bool Storage::saveItems(const std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key) {
    spdlog::info("Saving {} encrypted items to '{}'", items.size(), filename);
    if (key.size() != crypto_secretbox_KEYBYTES) {
        spdlog::error("Invalid key size");
        return false;
    }

    std::string plain = serializeItemsPlain(items);
    const unsigned char* p = reinterpret_cast<const unsigned char*>(plain.data());
    unsigned long long plen = plain.size();

    std::vector<unsigned char> ciphertext(plen + crypto_secretbox_MACBYTES);

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    if (crypto_secretbox_easy(ciphertext.data(), p, plen, nonce, key.data()) != 0) {
        spdlog::error("Encryption failed");
        return false;
    }

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("Failed to open '{}' for encrypted write", filename);
        return false;
    }

    out.write(MAGIC_HDR, sizeof(MAGIC_HDR) - 1);
    out.write(reinterpret_cast<const char*>(nonce), sizeof(nonce));
    out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    return true;
}

bool Storage::loadItems(std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key) {
    spdlog::info("Loading encrypted items from '{}'", filename);
    items.clear();

    if (key.size() != crypto_secretbox_KEYBYTES) {
        spdlog::error("Invalid key size");
        return false;
    }

    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        spdlog::warn("Item file '{}' not found; treating as empty", filename);
        return true;
    }

    char hdr[sizeof(MAGIC_HDR) - 1];
    in.read(hdr, sizeof(hdr));
    if (in.gcount() != sizeof(hdr) || std::strncmp(hdr, MAGIC_HDR, sizeof(hdr)) != 0) {
        spdlog::error("Invalid magic header");
        return false;
    }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    in.read(reinterpret_cast<char*>(nonce), sizeof(nonce));
    if (in.gcount() != sizeof(nonce)) {
        spdlog::error("Failed to read nonce");
        return false;
    }

    std::vector<unsigned char> ciphertext(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    if (ciphertext.size() < crypto_secretbox_MACBYTES) {
        spdlog::error("Ciphertext too short");
        return false;
    }

    std::vector<unsigned char> plain(ciphertext.size() - crypto_secretbox_MACBYTES);
    if (crypto_secretbox_open_easy(plain.data(), ciphertext.data(), ciphertext.size(), nonce, key.data()) != 0) {
        spdlog::error("Decryption failed");
        return false;
    }

    std::string plain_str(reinterpret_cast<char*>(plain.data()), plain.size());
    parsePlainToItems(plain_str, items);

    spdlog::info("Loaded {} items", items.size());
    return true;
}

bool Storage::saveTagWeights(const TagManager& mgr, const std::string& filename, const std::vector<unsigned char>& key) {
    spdlog::info("Saving tag weights to '{}'", filename);

    if (key.size() != crypto_secretbox_KEYBYTES) {
        spdlog::error("Invalid key size");
        return false;
    }

    std::string plain = mgr.serialize();
    const unsigned char* p = reinterpret_cast<const unsigned char*>(plain.data());
    unsigned long long plen = plain.size();

    std::vector<unsigned char> ciphertext(plen + crypto_secretbox_MACBYTES);

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    if (crypto_secretbox_easy(ciphertext.data(), p, plen, nonce, key.data()) != 0) {
        spdlog::error("crypto_secretbox_easy failed");
        return false;
    }

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("Failed to write tag weights");
        return false;
    }

    out.write(MAGIC_HDR, sizeof(MAGIC_HDR) - 1);
    out.write(reinterpret_cast<const char*>(nonce), sizeof(nonce));
    out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    return true;
}

bool Storage::loadTagWeights(TagManager& mgr, const std::string& filename, const std::vector<unsigned char>& key) {
    spdlog::info("Loading tag weights from '{}'", filename);
    mgr.weights.clear();

    if (key.size() != crypto_secretbox_KEYBYTES) {
        spdlog::error("Invalid key size");
        return false;
    }

    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        spdlog::warn("Tag weight file '{}' not found; using defaults", filename);
        return true;
    }

    char hdr[sizeof(MAGIC_HDR) - 1];
    in.read(hdr, sizeof(hdr));
    if (in.gcount() != sizeof(hdr) || std::strncmp(hdr, MAGIC_HDR, sizeof(hdr)) != 0) {
        spdlog::error("Invalid tag weight header");
        return false;
    }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    in.read(reinterpret_cast<char*>(nonce), sizeof(nonce));
    if (in.gcount() != sizeof(nonce)) {
        spdlog::error("Failed to read nonce");
        return false;
    }

    std::vector<unsigned char> ciphertext(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    if (ciphertext.size() < crypto_secretbox_MACBYTES) {
        spdlog::error("Ciphertext too short");
        return false;
    }

    unsigned long long clen = ciphertext.size();
    unsigned long long plen = clen - crypto_secretbox_MACBYTES;
    std::vector<unsigned char> plain(plen);

    if (crypto_secretbox_open_easy(plain.data(), ciphertext.data(), clen, nonce, key.data()) != 0) {
        spdlog::error("Decryption failed");
        return false;
    }

    std::string plain_str(reinterpret_cast<const char*>(plain.data()), plain.size());
    mgr.deserialize(plain_str);

    spdlog::info("Loaded {} tag weights", mgr.weights.size());
    return true;
}
