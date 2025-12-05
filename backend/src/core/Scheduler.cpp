#include "Scheduler.hpp"
#include <ctime>
#include <algorithm>
#include <cmath>
#include <queue>

Scheduler::Scheduler(TagManager* tagMgr)
    : tagManager(tagMgr),
    ease_min(1.3),
    ease_max(2.8),
    lapse_reset_interval_days(1),
    leech_threshold(8),
    // FSRS-inspired targets / bounds
    fsrs_q_target_good(0.90),
    fsrs_q_target_hard(0.80),
    fsrs_q_target_easy(0.95),
    fsrs_max_stability(36500.0), // extremely long maximum stability (days)
    fsrs_min_stability(0.5)
{
    spdlog::info("Scheduler (hybrid FSRS/SM2) initialized with tag priority support.");
}

/*
  Public API:
    - review(Item&, ReviewQuality)
    - getDueItems(vector<Item>&)
*/

void Scheduler::review(Item& item, ReviewQuality q) {
    spdlog::info("Review Item {} | q={}", item.title, static_cast<int>(q));

    // Acquire or create metadata for this item
    Meta& m = meta_by_item[&item];

    // If AGAIN, treat as lapse
    if (q == ReviewQuality::AGAIN) {
        handleLapse(item, m);
        // record last review time
        m.last_review = std::time(nullptr);
        return;
    }

    // Ensure we have sensible initial values if this is the first time we see the item
    if (m.reps == 0) {
        // If item has a non-zero interval or EF already, we start from that
        m.stability = std::max(fsrs_min_stability, static_cast<double>(std::max(1, item.interval)));
        // difficulty: infer from ease_factor if available (ease_factor ~ 2.5 -> easier)
        // Map EF in [1.3, 2.8] -> difficulty in [0.15,0.85] roughly (higher difficulty -> harder)
        double ef = item.ease_factor;
        m.difficulty = 1.0 - ((ef - ease_min) / (ease_max - ease_min)); // naive mapping
        m.difficulty = std::clamp(m.difficulty, 0.05, 0.95);
    }

    // Use FSRS-inspired algorithm when we have at least one recorded rep; else use SM2 fallback
    int proposedInterval = 1;
    if (m.reps >= 1) {
        proposedInterval = computeNewIntervalFSRS(item, m, q);
    }
    else {
        proposedInterval = computeNewIntervalSM2(item, q);
    }

    // Update metadata: stability & difficulty get updated by FSRS function (we call it)
    // computeNewIntervalFSRS already updates stability/difficulty in the Meta object.
    // For SM2 fallback, we still bump reps and lightly update difficulty.
    if (m.reps == 0) {
        // small difficulty update for first successful review
        updateDifficulty(m, q);
    }

    // increment repetition count & set last review
    m.reps += 1;
    m.last_review = std::time(nullptr);

    // apply tag priority which shortens interval multiplicatively for important tags
    int withTagPriority = applyTagPriority(item, proposedInterval);

    // clamp to at least 1 day
    withTagPriority = std::max(1, withTagPriority);

    // update item's ease_factor using a conservative formula similar to SM2 but clamped
    double newEF = item.ease_factor;
    switch (q) {
    case ReviewQuality::HARD: newEF = item.ease_factor - 0.05; break;
    case ReviewQuality::GOOD: newEF = item.ease_factor + 0.01; break;
    case ReviewQuality::EASY: newEF = item.ease_factor + 0.15; break;
    default: break;
    }
    item.ease_factor = std::clamp(newEF, ease_min, ease_max);

    // schedule the item (assume Item::scheduleNext accepts days)
    item.scheduleNext(withTagPriority);
}

/*
  Get due items from an input list.
  Preserves the same return signature (vector<Item*>). We'll return items whose
  next_review is <= now, sorted by:
    1. highest tag priority (so heavy-weight tags come first),
    2. earliest next_review (older due first).
*/
std::vector<Item*> Scheduler::getDueItems(std::vector<Item>& items) const {
    std::vector<Item*> due;
    std::time_t now = std::time(nullptr);
    due.reserve(items.size() / 4 + 8);

    for (auto& item : items) {
        if (item.next_review <= now) {
            due.push_back(&item);
        }
    }

    // Sort by combined tag weight desc, then next_review asc
    std::sort(due.begin(), due.end(),
        [this](const Item* a, const Item* b) {
            double wa = combinedTagWeight(*a);
            double wb = combinedTagWeight(*b);
            if (wa != wb) return wa > wb;
            return a->next_review < b->next_review;
        });

    return due;
}

/* -------------------------
   FSRS-inspired model
   -------------------------
   We use a simple, robust FSRS-style update:

   - Retention model: R(t) = exp(-t / stability)  (stability in days)
     If we want a target retention p*, then choose t_next = -stability * ln(p*)

   - After a successful review with response quality q we update stability multiplicatively:
       stability_new = stability * f(q, difficulty)
     where f grows with q (easy -> large growth), and shrinks with difficulty

   - Difficulty nudges toward easier after good reviews, and toward harder on borderline reviews.
   - For items with very small history we fallback to SM-2 like multiplier using item.interval & item.ease_factor.

   The algorithm returns interval in whole days.
*/

int Scheduler::computeNewIntervalFSRS(const Item& item, Meta& m, ReviewQuality q) const {
    // derive target retention p* for the given quality
    double p_target = fsrs_q_target_good;
    if (q == ReviewQuality::HARD) p_target = fsrs_q_target_hard;
    else if (q == ReviewQuality::EASY) p_target = fsrs_q_target_easy;

    // ensure stability is within bounds
    m.stability = std::clamp(m.stability, fsrs_min_stability, fsrs_max_stability);

    // compute next interval (days) using R(t) = exp(-t / s) -> t = -s * ln(p_target)
    // if p_target >= 1.0 numeric issues; clamp
    p_target = std::clamp(p_target, 0.01, 0.999999);

    double next_interval = -m.stability * std::log(p_target);

    // ensure a minimum of 1 day
    int interval_days = std::max(1, static_cast<int>(std::ceil(next_interval)));

    // Update stability using a multiplicative update proportional to quality and inversely to difficulty
    double stability_before = m.stability;
    double growth_factor = updateStability(m, q, interval_days);
    // clamp stability after update
    m.stability = std::clamp(m.stability, fsrs_min_stability, fsrs_max_stability);

    // Update difficulty
    updateDifficulty(m, q);

    spdlog::debug("FSRS compute: item='{}' old_stab={:.3f} growth={:.3f} new_stab={:.3f} diff={:.3f} -> interval={}d",
        item.title, stability_before, growth_factor, m.stability, m.difficulty, interval_days);

    // Safety: if result is absurdly large ( > 50 years ), cap to something reasonable
    const int cap_days = 365 * 50;
    if (interval_days > cap_days) interval_days = cap_days;

    return interval_days;
}

double Scheduler::updateStability(Meta& m, ReviewQuality q, int interval) const {
    // We update stability multiplicatively.
    // Base growth per quality (GOOD/EASY/HARD)
    double base = 1.0;
    if (q == ReviewQuality::HARD) base = 1.15;
    else if (q == ReviewQuality::GOOD) base = 1.8;
    else if (q == ReviewQuality::EASY) base = 2.8;

    // Difficulty reduces growth: higher difficulty -> lower growth
    double difficulty_factor = 1.0 - (m.difficulty * 0.5); // difficulty in [0..1]
    difficulty_factor = std::clamp(difficulty_factor, 0.4, 1.0);

    // Slight dependence on current interval: learning plateaus on long intervals
    double interval_factor = 1.0 + std::log(1.0 + std::max(1.0, static_cast<double>(interval))) * 0.05;

    double growth = base * difficulty_factor * interval_factor;

    // Apply growth to stability
    // If stability is very small, be conservative (avoid jump to huge stability)
    double new_stability = m.stability * growth;

    // Damp growth if growth would exceed a soft threshold
    if (new_stability > m.stability + 1000.0) {
        new_stability = m.stability + 1000.0;
    }

    m.stability = new_stability;
    return growth;
}

void Scheduler::updateDifficulty(Meta& m, ReviewQuality q) const {
    // difficulty in [0..1] (higher => harder)
    // GOOD -> slightly reduce difficulty, HARD -> increase slightly, EASY -> reduce more
    double delta = 0.0;
    if (q == ReviewQuality::HARD) delta = 0.03;
    else if (q == ReviewQuality::GOOD) delta = -0.01;
    else if (q == ReviewQuality::EASY) delta = -0.05;

    m.difficulty = std::clamp(m.difficulty + delta, 0.01, 0.99);
}

/* -------------------------
   SM-2 style fallback
   -------------------------
   We use a simple and safe SM-2 style multiplier when there isn't enough
   FSRS history (m.reps == 0). This keeps behavior similar to classic SRS.
*/
int Scheduler::computeNewIntervalSM2(const Item& item, ReviewQuality q) const {
    // Use item's interval and EF to produce a next interval.
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

/* -------------------------
   Tag priority
   -------------------------
   We compute a combined tag weight from TagManager:
     - TagManager::getWeight(tag) returns an integer weight >= 1
     - combine multiple tag weights multiplicatively but softly (geometric mean style)
   Then we **shorten** intervals for higher weight tags by dividing interval by pow(scale, weight-1),
   producing more frequent reviews for high priority tags.

   This behaviour can be tuned: currently each extra weight reduces the interval by factor ~1.2
*/
double Scheduler::combinedTagWeight(const Item& item) const {
    if (!tagManager) return 1.0;

    // collect weights (>=1)
    double prod = 1.0;
    int count = 0;
    for (const auto& t : item.tags) {
        int w = tagManager->getWeight(t);
        w = std::max(1, w);
        prod *= static_cast<double>(w);
        ++count;
    }
    if (count == 0) return 1.0;

    // geometric mean of weights
    double geom_mean = std::pow(prod, 1.0 / static_cast<double>(count));
    return std::max(1.0, geom_mean);
}

int Scheduler::applyTagPriority(const Item& item, int interval) const {
    double weight = combinedTagWeight(item); // >= 1.0
    if (weight <= 1.0) return interval;

    // scale base (each 1.0 increase in weight shortens interval by ~1.2 factor)
    const double perWeightScale = 1.20;

    // compute divisor = perWeightScale^(weight - 1)
    double divisor = std::pow(perWeightScale, weight - 1.0);
    int adjusted = static_cast<int>(std::ceil(static_cast<double>(interval) / divisor));

    // never extend interval due to tag priority; only shorten or keep same
    adjusted = std::max(1, std::min(interval, adjusted));
    return adjusted;
}

/* -------------------------
   Lapse & leech handling
   -------------------------
   When an item lapses (AGAIN), we:
    - increment lapses and set is_leech if beyond threshold
    - reduce ease_factor
    - reset schedule to a short interval (lapse_reset_interval_days)
    - update metadata stability to be small (so FSRS will treat it as recently failed)
*/
void Scheduler::handleLapse(Item& item, Meta& m) {
    item.lapses++;
    if (item.lapses >= leech_threshold) {
        item.is_leech = true;
    }

    // Penalize ease factor more strongly on lapse
    item.ease_factor = std::max(ease_min, item.ease_factor - 0.25);

    // Reset schedule to a short interval
    item.scheduleNext(lapse_reset_interval_days);

    // FSRS meta adjustments: reduce stability for this item so it re-learns
    m.stability = std::max(fsrs_min_stability, m.stability * 0.3);
    // If after many lapses mark as leech and avoid large stability growth
    if (item.is_leech) {
        m.stability = std::min(m.stability, 2.0); // keep it small
    }

    spdlog::warn("Item '{}' lapsed. lapses={}, is_leech={}", item.title, item.lapses, item.is_leech);
}
