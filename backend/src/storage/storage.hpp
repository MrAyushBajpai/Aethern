#pragma once
#include <vector>
#include <string>
#include "../core/Item.hpp"
#include "../auth/User.hpp"

class Storage {
public:
    // ----- USERS -----
    static bool saveUsers(const std::vector<User>& users, const std::string& filename);
    static bool loadUsers(std::vector<User>& users, const std::string& filename);

    // ----- ITEMS -----
    static bool saveItems(const std::vector<Item>& items, const std::string& filename);
    static bool loadItems(std::vector<Item>& items, const std::string& filename);
};
