#pragma once
#include <vector>
#include <unordered_map>
#include <ctime>
#include <algorithm>
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
    explicit Scheduler(TagManager* tagMgr);

    void review(Item& item, ReviewQuality quality);
    std::vector<Item*> getDueItems(std::vector<Item>& items) const;

private:
    TagManager* tagManager;

    struct Meta {
        int reps = 0;
        double stability = 1.0;
        double difficulty = 0.3;
        std::time_t last_review = 0;
    };

    mutable std::unordered_map<const Item*, Meta> meta_by_item;

    int computeNewIntervalFSRS(const Item& item, Meta& m, ReviewQuality q) const;
    int computeNewIntervalSM2(const Item& item, ReviewQuality q) const;
    double updateStability(Meta& m, ReviewQuality q, int interval) const;
    void updateDifficulty(Meta& m, ReviewQuality q) const;

    int applyTagPriority(const Item& item, int interval) const;
    double combinedTagWeight(const Item& item) const;

    void handleLapse(Item& item, Meta& m);

    double ease_min;
    double ease_max;
    int lapse_reset_interval_days;
    int leech_threshold;

    double fsrs_q_target_good;
    double fsrs_q_target_hard;
    double fsrs_q_target_easy;
    double fsrs_max_stability;
    double fsrs_min_stability;
};
