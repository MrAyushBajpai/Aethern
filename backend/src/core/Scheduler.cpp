#include "Scheduler.hpp"
#include <ctime>
#include <cmath>
#include <algorithm>

Scheduler::Scheduler(TagManager* tagMgr)
    : tagManager(tagMgr),
    ease_min(1.3),
    ease_max(2.8),
    lapse_reset_interval_days(1),
    leech_threshold(8),
    fsrs_q_target_good(0.90),
    fsrs_q_target_hard(0.80),
    fsrs_q_target_easy(0.95),
    fsrs_max_stability(36500.0),
    fsrs_min_stability(0.5)
{
    spdlog::info("Scheduler initialized.");
}

void Scheduler::review(Item& item, ReviewQuality q) {
    spdlog::info("Review Item {} | q={}", item.title, static_cast<int>(q));
    Meta& m = meta_by_item[&item];

    if (q == ReviewQuality::AGAIN) {
        handleLapse(item, m);
        m.last_review = std::time(nullptr);
        return;
    }

    if (m.reps == 0) {
        m.stability = std::max(fsrs_min_stability, static_cast<double>(std::max(1, item.interval)));
        double ef = item.ease_factor;
        m.difficulty = 1.0 - ((ef - ease_min) / (ease_max - ease_min));
        m.difficulty = std::clamp(m.difficulty, 0.05, 0.95);
    }

    int proposed =
        (m.reps >= 1)
        ? computeNewIntervalFSRS(item, m, q)
        : computeNewIntervalSM2(item, q);

    if (m.reps == 0) {
        updateDifficulty(m, q);
    }

    m.reps += 1;
    m.last_review = std::time(nullptr);

    int adjusted = applyTagPriority(item, proposed);
    adjusted = std::max(1, adjusted);

    double newEF = item.ease_factor;
    switch (q) {
    case ReviewQuality::HARD: newEF -= 0.05; break;
    case ReviewQuality::GOOD: newEF += 0.01; break;
    case ReviewQuality::EASY: newEF += 0.15; break;
    default: break;
    }
    item.ease_factor = std::clamp(newEF, ease_min, ease_max);

    item.history.push_back({
    std::time(nullptr),
    static_cast<int>(q),
    adjusted
        });

    item.scheduleNext(adjusted);
}

std::vector<Item*> Scheduler::getDueItems(std::vector<Item>& items) const {
    std::vector<Item*> due;
    std::time_t now = std::time(nullptr);
    due.reserve(items.size() / 4 + 8);

    for (auto& item : items) {
        if (item.next_review <= now) due.push_back(&item);
    }

    std::sort(due.begin(), due.end(),
        [this](const Item* a, const Item* b) {
            double wa = combinedTagWeight(*a);
            double wb = combinedTagWeight(*b);
            if (wa != wb) return wa > wb;
            return a->next_review < b->next_review;
        });

    return due;
}

int Scheduler::computeNewIntervalFSRS(const Item& item, Meta& m, ReviewQuality q) const {
    double p = fsrs_q_target_good;
    if (q == ReviewQuality::HARD) p = fsrs_q_target_hard;
    else if (q == ReviewQuality::EASY) p = fsrs_q_target_easy;

    m.stability = std::clamp(m.stability, fsrs_min_stability, fsrs_max_stability);

    p = std::clamp(p, 0.01, 0.999999);
    double next_interval = -m.stability * std::log(p);
    int interval_days = std::max(1, static_cast<int>(std::ceil(next_interval)));

    double before = m.stability;
    double growth = updateStability(m, q, interval_days);
    m.stability = std::clamp(m.stability, fsrs_min_stability, fsrs_max_stability);

    updateDifficulty(m, q);

    spdlog::debug(
        "FSRS: '{}' stab_old={:.3f} growth={:.3f} stab_new={:.3f} diff={:.3f} interval={}d",
        item.title, before, growth, m.stability, m.difficulty, interval_days
    );

    const int cap = 365 * 50;
    return std::min(interval_days, cap);
}

double Scheduler::updateStability(Meta& m, ReviewQuality q, int interval) const {
    double base = 1.0;
    if (q == ReviewQuality::HARD) base = 1.15;
    else if (q == ReviewQuality::GOOD) base = 1.8;
    else if (q == ReviewQuality::EASY) base = 2.8;

    double difficulty_factor = 1.0 - (m.difficulty * 0.5);
    difficulty_factor = std::clamp(difficulty_factor, 0.4, 1.0);

    double interval_factor =
        1.0 + std::log(1.0 + std::max(1.0, static_cast<double>(interval))) * 0.05;

    double growth = base * difficulty_factor * interval_factor;
    double new_stab = m.stability * growth;

    if (new_stab > m.stability + 1000.0) new_stab = m.stability + 1000.0;
    m.stability = new_stab;

    return growth;
}

void Scheduler::updateDifficulty(Meta& m, ReviewQuality q) const {
    double delta = 0.0;
    if (q == ReviewQuality::HARD) delta = 0.03;
    else if (q == ReviewQuality::GOOD) delta = -0.01;
    else if (q == ReviewQuality::EASY) delta = -0.05;

    m.difficulty = std::clamp(m.difficulty + delta, 0.01, 0.99);
}

int Scheduler::computeNewIntervalSM2(const Item& item, ReviewQuality q) const {
    double ef = item.ease_factor;
    int interval = std::max(1, item.interval);

    switch (q) {
    case ReviewQuality::HARD:
        interval = std::max(1, static_cast<int>(std::ceil(interval * (ef * 0.9))));
        break;
    case ReviewQuality::GOOD:
        interval = std::max(1, static_cast<int>(std::ceil(interval * ef)));
        break;
    case ReviewQuality::EASY:
        interval = std::max(1, static_cast<int>(std::ceil(interval * ef * 1.3)));
        break;
    default:
        break;
    }

    return interval;
}

double Scheduler::combinedTagWeight(const Item& item) const {
    if (!tagManager) return 1.0;

    double prod = 1.0;
    int count = 0;

    for (const auto& t : item.tags) {
        int w = std::max(1, tagManager->getWeight(t));
        prod *= static_cast<double>(w);
        count++;
    }

    if (count == 0) return 1.0;
    return std::max(1.0, std::pow(prod, 1.0 / static_cast<double>(count)));
}

int Scheduler::applyTagPriority(const Item& item, int interval) const {
    double weight = combinedTagWeight(item);
    if (weight <= 1.0) return interval;

    double divisor = std::pow(1.20, weight - 1.0);
    int adjusted = static_cast<int>(std::ceil(interval / divisor));
    return std::max(1, std::min(interval, adjusted));
}

void Scheduler::handleLapse(Item& item, Meta& m) {
    item.lapses++;
    if (item.lapses >= leech_threshold) item.is_leech = true;

    item.ease_factor = std::max(ease_min, item.ease_factor - 0.25);
    item.scheduleNext(lapse_reset_interval_days);

    m.stability = std::max(fsrs_min_stability, m.stability * 0.3);
    if (item.is_leech) m.stability = std::min(m.stability, 2.0);

    spdlog::warn("Item '{}' lapsed. lapses={}, is_leech={}", item.title, item.lapses, item.is_leech);
}
