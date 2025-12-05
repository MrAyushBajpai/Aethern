#include "Storage.hpp"
#include <fstream>

bool Storage::save(const std::vector<Item>& items, const std::string& filename) {
    std::ofstream out(filename);
    if (!out) return false;

    for (const auto& it : items) {
        out << it.title << "\n";
        out << it.content << "\n";
        out << it.interval << "\n";
        out << it.ease_factor << "\n";
        out << it.last_review << "\n";
        out << it.next_review << "\n";
        out << "---\n";
    }
    return true;
}

bool Storage::load(std::vector<Item>& items, const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return false;

    items.clear();

    while (true) {
        Item it;
        if (!std::getline(in, it.title)) break;
        std::getline(in, it.content);
        in >> it.interval;
        in >> it.ease_factor;
        in >> it.last_review;
        in >> it.next_review;

        std::string sep;
        std::getline(in, sep); // line break
        std::getline(in, sep); // read '---'

        items.push_back(it);
    }

    return true;
}
