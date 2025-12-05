#include "Scheduler.hpp"
#include <ctime>

void Scheduler::review(Item& item, ReviewQuality q) {
    if (q == ReviewQuality::AGAIN) {
        item.scheduleNext(1);
        return;
    }

    // Very simplified SM-2-like logic
    if (q == ReviewQuality::HARD)
        item.ease_factor -= 0.05;
    else if (q == ReviewQuality::GOOD)
        item.ease_factor += 0.01;
    else if (q == ReviewQuality::EASY)
        item.ease_factor += 0.15;

    if (item.ease_factor < 1.3)
        item.ease_factor = 1.3;

    int new_interval = static_cast<int>(item.interval * item.ease_factor);
    if (new_interval < 1) new_interval = 1;

    item.scheduleNext(new_interval);
}

std::vector<Item*> Scheduler::getDueItems(std::vector<Item>& items) const {
    std::vector<Item*> due;
    std::time_t now = std::time(nullptr);

    for (auto& item : items) {
        if (item.next_review <= now)
            due.push_back(&item);
    }
    return due;
}
