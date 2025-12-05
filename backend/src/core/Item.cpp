#include "Item.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

Item::Item(const std::string& t, const std::string& c)
    : title(t), content(c)
{
    id = generateID();
    last_review = std::time(nullptr);
    next_review = last_review + 24 * 60 * 60; // Default: tomorrow
    spdlog::info("Created Item: ID={}, Title={}", id, title);
}

void Item::scheduleNext(int days) {
    using namespace std::chrono;

    interval = days;
    last_review = std::time(nullptr);

    auto now = system_clock::now();
    auto future = now + hours(24 * days);

    next_review = system_clock::to_time_t(future);

    spdlog::info("Item ID={} scheduled: interval={} days, next_review={}",
        id, interval, next_review);
}

void Item::addTag(const std::string& tag) {
    if (tag.empty()) return;
    std::string t = tag;
    // trim spaces
    while (!t.empty() && std::isspace((unsigned char)t.front())) t.erase(t.begin());
    while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();
    if (t.empty()) return;

    if (!hasTag(t)) {
        tags.push_back(t);
        spdlog::debug("Item ID={} addTag '{}'", id, t);
    }
}

bool Item::removeTag(const std::string& tag) {
    auto it = std::find(tags.begin(), tags.end(), tag);
    if (it != tags.end()) {
        tags.erase(it);
        spdlog::debug("Item ID={} removeTag '{}'", id, tag);
        return true;
    }
    return false;
}

bool Item::hasTag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

void Item::setTags(const std::vector<std::string>& newTags) {
    tags.clear();
    for (const auto& t : newTags) {
        if (!t.empty()) {
            // trim simple whitespace
            std::string tmp = t;
            while (!tmp.empty() && std::isspace((unsigned char)tmp.front())) tmp.erase(tmp.begin());
            while (!tmp.empty() && std::isspace((unsigned char)tmp.back())) tmp.pop_back();
            if (!tmp.empty()) tags.push_back(tmp);
        }
    }
    spdlog::debug("Item ID={} setTags count={}", id, tags.size());
}

std::string Item::tagsAsLine() const {
    std::ostringstream oss;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i) oss << ",";
        oss << tags[i];
    }
    return oss.str();
}

// Simple unique ID generator (timestamp + random bits)
std::string Item::generateID() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto millis = duration_cast<milliseconds>(now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<uint64_t> dist;

    uint64_t randPart = dist(eng);

    std::stringstream ss;
    ss << std::hex << millis << "-" << std::setw(16) << std::setfill('0') << randPart;
    return ss.str();
}
