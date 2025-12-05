#include "Storage.hpp"
#include <fstream>
#include <iostream>

//
// USERS
//

bool Storage::saveUsers(const std::vector<User>& users, const std::string& filename) {
    std::ofstream out(filename);
    if (!out) return false;

    for (const auto& u : users) {
        out << u.username << "\n";
        out << u.password_hash << "\n";
        out << u.created_at << "\n";
        out << "---\n";
    }

    return true;
}

bool Storage::loadUsers(std::vector<User>& users, const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return false;

    users.clear();

    while (true) {
        User u;

        if (!std::getline(in, u.username)) break;
        std::getline(in, u.password_hash);
        in >> u.created_at;

        std::string sep;
        std::getline(in, sep);
        std::getline(in, sep);

        users.push_back(u);
    }
    return true;
}

//
// ITEMS
//

bool Storage::saveItems(const std::vector<Item>& items, const std::string& filename) {
    std::ofstream out(filename);
    if (!out) return false;

    for (const auto& it : items) {
        out << it.title << "\n";
        out << it.content << "\n";
        out << it.interval << "\n";
        out << it.ease_factor << "\n";
        out << it.last_review << "\n";
        out << it.next_review << "\n";
        out << "---\n";
    }

    return true;
}

bool Storage::loadItems(std::vector<Item>& items, const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return false;

    items.clear();

    while (true) {
        Item it;

        if (!std::getline(in, it.title)) break;
        std::getline(in, it.content);
        in >> it.interval;
        in >> it.ease_factor;
        in >> it.last_review;
        in >> it.next_review;

        std::string sep;
        std::getline(in, sep);
        std::getline(in, sep);

        items.push_back(it);
    }
    return true;
}
