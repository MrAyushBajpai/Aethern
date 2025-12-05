#pragma once
#include <vector>
#include <string>
#include "../core/Item.hpp"
#include "../auth/User.hpp"
#include "../core/TagManager.hpp"

class Storage {
public:
    static bool saveUsers(const std::vector<User>& users, const std::string& filename);
    static bool loadUsers(std::vector<User>& users, const std::string& filename);

    static bool saveItems(const std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key);
    static bool loadItems(std::vector<Item>& items, const std::string& filename, const std::vector<unsigned char>& key);

    static bool saveTagWeights(const TagManager& mgr, const std::string& filename, const std::vector<unsigned char>& key);
    static bool loadTagWeights(TagManager& mgr, const std::string& filename, const std::vector<unsigned char>& key);
};
