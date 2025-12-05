#pragma once
#include <string>
#include <ctime>
#include <chrono>
#include <spdlog/spdlog.h>

class Item {
public:
    Item() = default;
    Item(const std::string& title, const std::string& content = "");

    // Basic fields
    std::string id;          // Auto-generated
    std::string title;
    std::string content;

    // Scheduler state
    int interval = 1;             // Days
    double ease_factor = 2.5;
    std::time_t last_review = 0;  // Seconds since epoch
    std::time_t next_review = 0;

    // Enhanced fields
    int lapses = 0;               // Count of failures
    bool is_leech = false;        // Auto-flagged if too many lapses
    int review_count = 0;         // Total successful reviews
    int streak = 0;               // Consecutive successful reviews

    // Scheduling function (days into future)
    void scheduleNext(int days);

    // Utility
    static std::string generateID();
};
