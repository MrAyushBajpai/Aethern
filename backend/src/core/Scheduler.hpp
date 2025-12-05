#pragma once
#include <vector>
#include "Item.hpp"
#include <spdlog/spdlog.h>

enum class ReviewQuality {
    AGAIN = 1,
    HARD = 2,
    GOOD = 3,
    EASY = 4
};

class Scheduler {
public:
    Scheduler();

    void review(Item& item, ReviewQuality quality);

    // Return items whose next_review timestamp is <= now
    std::vector<Item*> getDueItems(std::vector<Item>& items) const;

private:
    // Internal helpers
    int computeNewInterval(const Item& item, ReviewQuality q) const;
    double computeEaseFactor(double EF, ReviewQuality q) const;
    void handleLapse(Item& item);

    // Configurable parameters
    double ease_min;
    double ease_max;
    int lapse_reset_interval;     // Reset interval when the user fails
    int leech_threshold;          // How many lapses before flagging as leech
};
