#pragma once
#include <string>
#include <ctime>

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

    void scheduleNext(int days);
};
