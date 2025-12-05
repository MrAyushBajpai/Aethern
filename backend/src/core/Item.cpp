#include "Item.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

Item::Item(const std::string& t, const std::string& c)
    : title(t), content(c)
{
    id = generateID();
    last_review = std::time(nullptr);
    next_review = last_review + 24 * 60 * 60;
    spdlog::info("Created Item: ID={}, Title={}", id, title);
}

static inline void trim(std::string& s) {
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
}

void Item::scheduleNext(int days) {
    using namespace std::chrono;

    interval = days;
    last_review = std::time(nullptr);

    next_review = system_clock::to_time_t(system_clock::now() + hours(24 * days));

    spdlog::info("Item ID={} scheduled: interval={} days, next_review={}",
        id, interval, next_review);
}

void Item::addTag(const std::string& tag) {
    if (tag.empty()) return;
    std::string t = tag;
    trim(t);
    if (t.empty() || hasTag(t)) return;
    tags.push_back(t);
    spdlog::debug("Item ID={} addTag '{}'", id, t);
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
    for (auto t : newTags) {
        trim(t);
        if (!t.empty()) tags.push_back(t);
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

std::string Item::generateID() {
    using namespace std::chrono;

    auto millis = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    std::random_device rd;
    std::mt19937_64 eng(rd());
    uint64_t randPart = std::uniform_int_distribution<uint64_t>()(eng);

    std::stringstream ss;
    ss << std::hex << millis << "-" << std::setw(16) << std::setfill('0') << randPart;
    return ss.str();
}
