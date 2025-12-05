#pragma once
#include <vector>
#include <string>
#include "../core/Item.hpp"

class Storage {
public:
    static bool save(const std::vector<Item>& items, const std::string& filename);
    static bool load(std::vector<Item>& items, const std::string& filename);
};
