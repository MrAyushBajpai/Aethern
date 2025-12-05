#pragma once
#include <string>
#include <ctime>
#include <vector>
#include <spdlog/spdlog.h>

class Item {
public:
    Item() = default;
    Item(const std::string& title, const std::string& content = "");

    std::string id;
    std::string title;
    std::string content;

    int interval = 1;
    double ease_factor = 2.5;
    std::time_t last_review = 0;
    std::time_t next_review = 0;

    int lapses = 0;
    bool is_leech = false;
    int review_count = 0;
    int streak = 0;

    std::vector<std::string> tags;

    void scheduleNext(int days);

    void addTag(const std::string& tag);
    bool removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    void setTags(const std::vector<std::string>& newTags);
    std::string tagsAsLine() const;

    static std::string generateID();
};
