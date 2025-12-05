#pragma once
#include <string>
#include <ctime>
#include <chrono>
#include <vector>
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

    // Tags
    std::vector<std::string> tags;

    // Scheduling function (days into future)
    void scheduleNext(int days);

    // Tag helpers
    void addTag(const std::string& tag);
    bool removeTag(const std::string& tag); // returns true if removed
    bool hasTag(const std::string& tag) const;
    void setTags(const std::vector<std::string>& newTags);
    std::string tagsAsLine() const; // CSV single line for storage

    // Utility
    static std::string generateID();
};
