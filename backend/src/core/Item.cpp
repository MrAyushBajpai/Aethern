#include "Item.hpp"
#include <chrono>

Item::Item(const std::string& t, const std::string& c)
    : title(t), content(c)
{
    last_review = std::time(nullptr);
    next_review = last_review + 24 * 60 * 60; // tomorrow
}

void Item::scheduleNext(int days) {
    interval = days;
    last_review = std::time(nullptr);
    next_review = last_review + days * 24 * 60 * 60;
}
