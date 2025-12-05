#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>
#include <cmath>
#include "TagManager.hpp"
#include "Item.hpp"

enum class ReviewQuality {
    AGAIN = 0,
    HARD = 1,
    GOOD = 2,
    EASY = 3
};

class Scheduler {
public:
    explicit Scheduler(TagManager* tags = nullptr)
        : tagManager(tags) {
    }

    void review(Item& item, ReviewQuality q);
    std::vector<Item*> getDueItems(std::vector<Item>& items) const;

private:
    struct SM2Data {
        int reps = 0;
        double ef = 2.5;
        int last_interval = 1;
    };

    TagManager* tagManager;
    std::unordered_map<std::string, SM2Data> cards;

    // Tag helpers
    double combinedTagWeight(const Item& item) const;
    int applyTagPriority(const Item& item, int interval) const;
};
