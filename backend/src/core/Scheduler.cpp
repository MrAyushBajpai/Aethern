#include "Scheduler.hpp"
#include <ctime>
#include <algorithm>

Scheduler::Scheduler(TagManager* tagMgr)
    : tagManager(tagMgr),
    ease_min(1.3),
    ease_max(2.8),
    lapse_reset_interval(1),
    leech_threshold(8)
{
    spdlog::info("Scheduler initialized with tag priority support.");
}

void Scheduler::review(Item& item, ReviewQuality q) {
    spdlog::info("Review Item {} | q={}", item.title, (int)q);

    if (q == ReviewQuality::AGAIN) {
        handleLapse(item);
        return;
    }

    double newEF = computeEaseFactor(item.ease_factor, q);
    item.ease_factor = std::clamp(newEF, ease_min, ease_max);

    int newInterval = computeNewInterval(item, q);
    newInterval = applyTagPriority(item, newInterval);

    item.scheduleNext(newInterval);
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

double Scheduler::computeEaseFactor(double EF, ReviewQuality q) const {
    switch (q) {
    case ReviewQuality::HARD: return EF - 0.05;
    case ReviewQuality::GOOD: return EF + 0.01;
    case ReviewQuality::EASY: return EF + 0.15;
    default: return EF;
    }
}

int Scheduler::computeNewInterval(const Item& item, ReviewQuality q) const {
    double ef = item.ease_factor;
    int interval = item.interval;

    switch (q) {
    case ReviewQuality::HARD: interval *= (ef * 0.9); break;
    case ReviewQuality::GOOD: interval *= ef; break;
    case ReviewQuality::EASY: interval *= ef * 1.3; break;
    default: break;
    }

    return std::max(1, interval);
}

int Scheduler::applyTagPriority(const Item& item, int interval) const {
    int highest = 1;

    for (auto& tag : item.tags) {
        int w = tagManager->getWeight(tag);
        if (w > highest) highest = w;
    }

    if (highest > 1)
        interval = std::max(1, interval / highest);

    return interval;
}

void Scheduler::handleLapse(Item& item) {
    item.lapses++;
    if (item.lapses >= leech_threshold)
        item.is_leech = true;

    item.ease_factor = std::max(ease_min, item.ease_factor - 0.2);
    item.scheduleNext(lapse_reset_interval);
}
