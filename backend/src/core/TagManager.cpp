#include "TagManager.hpp"
#include <sstream>

std::string TagManager::serialize() const {
    std::ostringstream oss;
    for (const auto& p : weights) {
        oss << p.first << ":" << p.second << "\n";
    }
    return oss.str();
}

void TagManager::deserialize(const std::string& data) {
    weights.clear();
    std::istringstream iss(data);
    std::string line;

    while (std::getline(iss, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string valStr = line.substr(pos + 1);
        if (key.empty()) continue;

        try {
            int val = std::stoi(valStr);
            if (val >= 1) weights[key] = val;
        }
        catch (...) {
            continue;
        }
    }
}
