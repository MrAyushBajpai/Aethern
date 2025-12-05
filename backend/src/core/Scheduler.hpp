#pragma once
#include <vector>
#include "Item.hpp"

enum class ReviewQuality {
    AGAIN = 1,
    HARD = 2,
    GOOD = 3,
    EASY = 4
};

class Scheduler {
public:
    void review(Item& item, ReviewQuality quality);
    std::vector<Item*> getDueItems(std::vector<Item>& items) const;
};
