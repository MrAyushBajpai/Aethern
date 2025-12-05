#include "Scheduler.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <ctime>
#include <algorithm>

// Map your 0..3 enum to SM-2 quality (1..5)
static inline int mapToSM2Quality(ReviewQuality q) {
    switch (q) {
    case ReviewQuality::AGAIN: return 1; // fail
    case ReviewQuality::HARD:  return 3; // correct but hard
    case ReviewQuality::GOOD:  return 4; // correct
    case ReviewQuality::EASY:  return 5; // very good
    default:                   return 3;
    }
}

void Scheduler::review(Item& item, ReviewQuality q) {
    SM2Data& data = cards[item.id];

    int smq = mapToSM2Quality(q);

    // Update EF (SuperMemo-2 formula)
    data.ef = data.ef + (0.1 - (5 - smq) * (0.08 + (5 - smq) * 0.02));
    if (data.ef < 1.3) data.ef = 1.3;

    int interval = 1;

    if (smq < 3) {
        // Failure or very poor recall
        data.reps = 0;
        interval = 1;
    }
    else {
        data.reps += 1;
        if (data.reps == 1) {
            interval = 1;
        }
        else if (data.reps == 2) {
            interval = 6;
        }
        else {
            // Use last_interval (days) and EF
            interval = std::max(1, static_cast<int>(std::round(data.last_interval * data.ef)));
        }
    }

    // Save last interval for next multiply
    data.last_interval = interval;

    // Apply tag priority shortening
    interval = applyTagPriority(item, interval);

    // Persist in item
    item.scheduleNext(interval);

    // store history with SM-2 quality (1..5) to be explicit
    item.history.push_back({ std::time(nullptr), smq, interval });

    spdlog::info("SM2 Review '{}' | smq={} | interval={} | ef={:.3f} | reps={}",
        item.title, smq, interval, data.ef, data.reps);
}

std::vector<Item*> Scheduler::getDueItems(std::vector<Item>& items) const {
    std::vector<Item*> out;
    std::time_t now = std::time(nullptr);

    for (auto& it : items)
        if (it.next_review <= now)
            out.push_back(&it);

    std::sort(out.begin(), out.end(),
        [&](const Item* a, const Item* b) {
            double wa = combinedTagWeight(*a);
            double wb = combinedTagWeight(*b);
            if (wa != wb) return wa > wb;
            return a->next_review < b->next_review;
        });

    return out;
}

double Scheduler::combinedTagWeight(const Item& item) const {
    if (!tagManager || item.tags.empty()) return 1.0;

    double sum = 0.0;
    int count = 0;
    for (const auto& t : item.tags) {
        int w = std::max(1, tagManager->getWeight(t));
        sum += static_cast<double>(w);
        ++count;
    }
    if (count == 0) return 1.0;
    return sum / static_cast<double>(count);
}

int Scheduler::applyTagPriority(const Item& item, int interval) const {
    double avgWeight = combinedTagWeight(item);
    if (avgWeight <= 1.0) return std::max(1, interval);

    // Convert avg weight into a modest shortening factor
    double factor = 1.0 + (avgWeight - 1.0) * 0.15;
    if (factor < 1.0) factor = 1.0;

    int adjusted = static_cast<int>(std::round(static_cast<double>(interval) / factor));
    adjusted = std::clamp(adjusted, 1, interval); // never lengthen; only shorten up to original
    return adjusted;
}
