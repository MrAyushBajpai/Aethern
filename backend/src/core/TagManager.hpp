#pragma once
#include <unordered_map>
#include <string>
#include <spdlog/spdlog.h>

class TagManager {
public:
    std::unordered_map<std::string, int> weights;

    int getWeight(const std::string& tag) const {
        auto it = weights.find(tag);
        return it == weights.end() ? 1 : it->second;
    }

    void setWeight(const std::string& tag, int weight) {
        if (weight < 1) weight = 1;
        weights[tag] = weight;
        spdlog::info("Tag '{}' weight set to {}", tag, weight);
    }

    void removeWeight(const std::string& tag) {
        weights.erase(tag);
        spdlog::info("Tag '{}' weight removed", tag);
    }

    std::string serialize() const;
    void deserialize(const std::string& data);
};
