#include "Item.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

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
