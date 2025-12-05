#include <iostream>
#include <vector>
#include <string>
#include <sodium.h>
#include <algorithm>
#include <limits>
#include <sstream>

#include "../utils/logging.hpp"
#include "../auth/AuthManager.hpp"
#include "../storage/Storage.hpp"
#include "../core/Scheduler.hpp"
#include "../core/TagManager.hpp"

std::string itemFileFor(const std::string& username) {
    return "data_" + username + ".dat";
}

std::string tagFileFor(const std::string& username) {
    return "tagdata_" + username + ".dat";
}

void listAllItems(const std::vector<Item>& items) {
    std::cout << "\n===== ALL ITEMS =====\n";

    if (items.empty()) {
        std::cout << "No items stored.\n";
        return;
    }

    for (size_t i = 0; i < items.size(); i++) {
        const Item& it = items[i];
        std::cout << i + 1 << ". " << it.title << "\n";

        std::cout << "   Tags: ";
        if (it.tags.empty()) std::cout << "(none)";
        else {
            for (size_t j = 0; j < it.tags.size(); ++j) {
                if (j) std::cout << ", ";
                std::cout << it.tags[j];
            }
        }
        std::cout << "\n";

        std::cout << "   Interval: " << it.interval << " days\n";
        std::cout << "   Ease: " << it.ease_factor << "\n";
        std::cout << "   Lapses: " << it.lapses << "\n";
        std::cout << "   Streak: " << it.streak << "\n";
        std::cout << "   Next review: " << it.next_review << " (UNIX)\n";

        std::cout << "   Review History:\n";
        if (it.history.empty()) {
            std::cout << "      (no history)\n";
        }
        else {
            for (const auto& r : it.history) {
                std::cout << "      - " << r.timestamp
                    << " | quality=" << r.quality
                    << " | interval_after=" << r.interval_after
                    << "\n";
            }
        }

        std::cout << "-----------------------------\n";
    }
}


int askQuality() {
    while (true) {
        std::cout << "\nChoose difficulty:\n"
            " 1 = AGAIN (Failed)\n"
            " 2 = HARD\n"
            " 3 = GOOD\n"
            " 4 = EASY\n> ";
        int q;
        if (std::cin >> q && q >= 1 && q <= 4) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return q;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid input.\n";
    }
}

std::vector<std::string> splitTagsLine(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream iss(line);
    std::string t;
    while (std::getline(iss, t, ',')) {
        while (!t.empty() && std::isspace((unsigned char)t.front())) t.erase(t.begin());
        while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

std::vector<std::string> gatherAllTags(const std::vector<Item>& items) {
    std::vector<std::string> all;
    for (const auto& it : items) {
        for (const auto& t : it.tags)
            if (std::find(all.begin(), all.end(), t) == all.end())
                all.push_back(t);
    }
    return all;
}

int chooseItemIndex(const std::vector<Item>& items) {
    if (items.empty()) {
        std::cout << "No items available.\n";
        return -1;
    }
    listAllItems(items);
    std::cout << "Choose item number: ";

    int sel;
    if (!(std::cin >> sel)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return -1;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (sel < 1 || (size_t)sel > items.size()) {
        std::cout << "Invalid selection.\n";
        return -1;
    }
    return sel - 1;
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

    Log::init();

    AuthManager auth;
    std::vector<Item> items;
    TagManager tagManager;
    User* current = nullptr;

    // LOGIN / SIGNUP
    while (!current) {
        std::cout << "\n===== LOGIN MENU =====\n"
            "1. Login\n"
            "2. Signup\n"
            "3. Exit\n> ";
        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::string dummy; std::getline(std::cin, dummy);
            continue;
        }
        std::cin.ignore();

        if (choice == 1) {
            std::string username, password;
            std::cout << "Username: "; std::getline(std::cin, username);
            std::cout << "Password: "; std::getline(std::cin, password);

            if (auth.login(username, password)) {
                current = auth.getCurrentUser();
                const auto& key = auth.getSessionKey();

                Storage::loadItems(items, itemFileFor(current->username), key);
                Storage::loadTagWeights(tagManager, tagFileFor(current->username), key);

                std::cout << "Login successful.\n";
            }
            else {
                std::cout << "Invalid username/password.\n";
            }
        }
        else if (choice == 2) {
            std::string username, password;
            std::cout << "Choose username: "; std::getline(std::cin, username);
            std::cout << "Choose password: "; std::getline(std::cin, password);
            if (username.empty() || password.empty()) { std::cout << "Empty fields.\n"; continue; }
            if (auth.signup(username, password)) std::cout << "Signup complete.\n";
            else std::cout << "Signup failed.\n";
        }
        else if (choice == 3)
            return 0;
    }

    // Scheduler (requires tagManager)
    Scheduler scheduler(&tagManager);

    // MAIN LOOP
    while (true) {
        std::cout << "\n===== MAIN MENU =====\n"
            "User: " << current->username << "\n"
            "1. Add Item\n"
            "2. Review Due Items\n"
            "3. List All Items\n"
            "4. Tag Management\n"
            "5. Save & Exit\n> ";

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear(); std::string dummy; std::getline(std::cin, dummy);
            continue;
        }
        std::cin.ignore();

        if (choice == 1) {
            std::string title, content, tags_line;
            std::cout << "Enter title: "; std::getline(std::cin, title);
            if (title.empty()) { std::cout << "Title required.\n"; continue; }

            std::cout << "Enter content: "; std::getline(std::cin, content);
            std::cout << "Enter tags (comma-separated): "; std::getline(std::cin, tags_line);

            Item it(title, content);
            it.setTags(splitTagsLine(tags_line));
            items.push_back(it);

            std::cout << "Item added.\n";
        }

        else if (choice == 2) {
            auto due = scheduler.getDueItems(items);
            if (due.empty()) { std::cout << "No items due.\n"; continue; }

            for (auto* item : due) {
                std::cout << "\nReviewing: " << item->title << "\nContent: " << item->content << "\nTags: ";

                if (item->tags.empty()) std::cout << "(none)";
                else {
                    for (size_t j = 0; j < item->tags.size(); ++j) {
                        if (j) std::cout << ", ";
                        std::cout << item->tags[j];
                    }
                }
                std::cout << "\n";

                int q = askQuality();
                scheduler.review(*item, static_cast<ReviewQuality>(q - 1));

                if (q == 1) item->streak = 0;
                else { item->review_count++; item->streak++; }

                std::cout << "Updated.\n";
            }
        }

        else if (choice == 3) {
            listAllItems(items);
        }

        else if (choice == 4) {
            // TAG MANAGEMENT SUBMENU
            while (true) {
                std::cout << "\n=== TAG MANAGEMENT ===\n"
                    "1. Add/Change tags on an item\n"
                    "2. Remove tag from item\n"
                    "3. Delete tag globally\n"
                    "4. List items by tag\n"
                    "5. List all tags\n"
                    "6. Set tag weight\n"
                    "7. Remove tag weight\n"
                    "8. List tag weights\n"
                    "9. Back\n> ";

                int t;
                if (!(std::cin >> t)) {
                    std::cin.clear(); std::string dummy; std::getline(std::cin, dummy);
                    continue;
                }
                std::cin.ignore();

                if (t == 1) {
                    int idx = chooseItemIndex(items); if (idx < 0) continue;
                    std::cout << "Enter new tags: ";
                    std::string line; std::getline(std::cin, line);
                    items[idx].setTags(splitTagsLine(line));
                }

                else if (t == 2) {
                    int idx = chooseItemIndex(items); if (idx < 0) continue;
                    std::cout << "Enter tag to remove: ";
                    std::string tag; std::getline(std::cin, tag);
                    if (!items[idx].removeTag(tag)) std::cout << "Not found.\n";
                }

                else if (t == 3) {
                    auto all = gatherAllTags(items);
                    if (all.empty()) { std::cout << "No tags.\n"; continue; }
                    std::cout << "Enter tag to remove globally: ";
                    std::string tg; std::getline(std::cin, tg);
                    int cnt = 0;
                    for (auto& it : items) if (it.removeTag(tg)) cnt++;
                    std::cout << "Removed from " << cnt << " item(s).\n";
                }

                else if (t == 4) {
                    auto all = gatherAllTags(items);
                    if (all.empty()) { std::cout << "No tags.\n"; continue; }
                    std::cout << "Enter tag: ";
                    std::string tag; std::getline(std::cin, tag);

                    std::vector<Item> filtered;
                    for (auto& it : items) if (it.hasTag(tag)) filtered.push_back(it);

                    listAllItems(filtered);
                }

                else if (t == 5) {
                    auto all = gatherAllTags(items);
                    if (all.empty()) std::cout << "No tags.\n";
                    else {
                        for (auto& t : all) std::cout << "- " << t << "\n";
                    }
                }

                else if (t == 6) { // set tag weight
                    std::cout << "Enter tag: ";
                    std::string tag; std::getline(std::cin, tag);
                    std::cout << "Enter weight (>=1): ";
                    int w; std::cin >> w; std::cin.ignore();
                    tagManager.setWeight(tag, w);
                }

                else if (t == 7) {
                    std::cout << "Enter tag: ";
                    std::string tag; std::getline(std::cin, tag);
                    tagManager.removeWeight(tag);
                }

                else if (t == 8) {
                    std::cout << "=== TAG WEIGHTS ===\n";
                    for (auto& p : tagManager.weights)
                        std::cout << p.first << " : " << p.second << "\n";
                }

                else if (t == 9)
                    break;

                else std::cout << "Invalid.\n";
            }
        }

        else if (choice == 5) {
            const auto& key = auth.getSessionKey();

            if (!Storage::saveItems(items, itemFileFor(current->username), key))
                std::cout << "Error saving items.\n";

            if (!Storage::saveTagWeights(tagManager, tagFileFor(current->username), key))
                std::cout << "Error saving tag weights.\n";

            auth.save();
            auth.logout();
            std::cout << "Goodbye!\n";
            break;
        }
    }

    return 0;
}
