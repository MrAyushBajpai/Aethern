#pragma once
#include <vector>
#include <spdlog/spdlog.h>
#include "Item.hpp"
#include "TagManager.hpp"

enum class ReviewQuality {
    AGAIN = 1,
    HARD = 2,
    GOOD = 3,
    EASY = 4
};

class Scheduler {
public:
    Scheduler(TagManager* tagMgr);

    void review(Item& item, ReviewQuality quality);
    std::vector<Item*> getDueItems(std::vector<Item>& items) const;

private:
    TagManager* tagManager;

    int computeNewInterval(const Item& item, ReviewQuality q) const;
    double computeEaseFactor(double EF, ReviewQuality q) const;
    int applyTagPriority(const Item& item, int interval) const;
    void handleLapse(Item& item);

    double ease_min;
    double ease_max;
    int lapse_reset_interval;
    int leech_threshold;
};
