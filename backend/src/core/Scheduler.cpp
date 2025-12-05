#include "Scheduler.hpp"
#include <ctime>
#include <algorithm>

Scheduler::Scheduler()
    : ease_min(1.3),
    ease_max(2.8),
    lapse_reset_interval(1),
    leech_threshold(8)
{
    spdlog::info("Scheduler initialized with ease_min={}, ease_max={}, lapse_reset_interval={}, leech_threshold={}",
        ease_min, ease_max, lapse_reset_interval, leech_threshold);
}

void Scheduler::review(Item& item, ReviewQuality q) {
    spdlog::info("Reviewing Item ID={} | Quality={}", item.id, static_cast<int>(q));

    // AGAIN: a lapse occurred
    if (q == ReviewQuality::AGAIN) {
        handleLapse(item);
        return;
    }

    // For successful reviews: modify ease factor
    double newEF = computeEaseFactor(item.ease_factor, q);

    spdlog::debug("Old EF={} -> New EF={}", item.ease_factor, newEF);

    item.ease_factor = std::clamp(newEF, ease_min, ease_max);

    // Compute new interval
    int newInterval = computeNewInterval(item, q);

    spdlog::debug("Old Interval={} | New Interval={}", item.interval, newInterval);

    item.scheduleNext(newInterval);
}

std::vector<Item*> Scheduler::getDueItems(std::vector<Item>& items) const {
    std::vector<Item*> due;
    std::time_t now = std::time(nullptr);

    for (auto& item : items) {
        if (item.next_review <= now) {
            due.push_back(&item);
            spdlog::debug("Item ID={} is due (next_review={}, now={})",
                item.id, item.next_review, now);
        }
    }

    spdlog::info("getDueItems: {} items due.", due.size());
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
    int interval = item.interval;

    if (interval <= 0) return 1;

    double ef = item.ease_factor;

    switch (q) {
    case ReviewQuality::HARD:
        interval = std::max(1, (int)(interval * (ef * 0.9)));
        break;
    case ReviewQuality::GOOD:
        interval = std::max(1, (int)(interval * ef));
        break;
    case ReviewQuality::EASY:
        interval = std::max(1, (int)(interval * ef * 1.3));
        break;
    default:
        break;
    }

    return interval;
}

void Scheduler::handleLapse(Item& item) {
    item.lapses++;
    spdlog::warn("Item ID={} Lapsed! Total lapses={}", item.id, item.lapses);

    // Leech detection
    if (item.lapses >= leech_threshold) {
        item.is_leech = true;
        spdlog::error("Item ID={} marked as LEECH ({} lapses)", item.id, item.lapses);
    }

    // Reset EF slightly
    item.ease_factor = std::max(ease_min, item.ease_factor - 0.2);
    spdlog::debug("Ease factor decreased due to lapse. Now EF={}", item.ease_factor);

    // Reset interval
    item.scheduleNext(lapse_reset_interval);
    spdlog::info("Lapse reset interval applied: next interval={}", lapse_reset_interval);
}
