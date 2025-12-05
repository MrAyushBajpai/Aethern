#pragma once
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cmath>
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

/*
  A drop-in replacement Scheduler offering:
   - SM-2 style fallback
   - FSRS-inspired stability/difficulty memory model maintained per-Item inside Scheduler
   - Tag-priority weighting (multiplicative scaling)
   - Leech detection & lapse handling
   - Efficient lookups using unordered_map; due-item sorting honors priority
*/

class Scheduler {
public:
    explicit Scheduler(TagManager* tagMgr);

    // Same public API as before (drop-in)
    void review(Item& item, ReviewQuality quality);
    std::vector<Item*> getDueItems(std::vector<Item>& items) const;

private:
    TagManager* tagManager;

    // Internal per-item metadata (we don't require changes to Item.hpp)
    struct Meta {
        int reps = 0;                 // *local* repetition counter maintained by Scheduler
        double stability = 1.0;       // FSRS-like stability (days)
        double difficulty = 0.3;      // [0..1], lower=easy, higher=hard; FSRS convention-ish
        std::time_t last_review = 0;  // epoch time of last review
    };
    mutable std::unordered_map<const Item*, Meta> meta_by_item;

    // Core algorithms
    int computeNewIntervalFSRS(const Item& item, Meta& m, ReviewQuality q) const;
    int computeNewIntervalSM2(const Item& item, ReviewQuality q) const; // fallback
    double updateStability(Meta& m, ReviewQuality q, int interval) const;
    void updateDifficulty(Meta& m, ReviewQuality q) const;

    // Tag priority application
    int applyTagPriority(const Item& item, int interval) const;
    double combinedTagWeight(const Item& item) const;

    // Lapse/leech handling
    void handleLapse(Item& item, Meta& m);

    // Helpers / constants (tuneable)
    double ease_min;
    double ease_max;
    int lapse_reset_interval_days;
    int leech_threshold;

    // FSRS tuning parameters
    double fsrs_q_target_good; // target retention probability for GOOD reviews
    double fsrs_q_target_hard;
    double fsrs_q_target_easy;
    double fsrs_max_stability;
    double fsrs_min_stability;
};
