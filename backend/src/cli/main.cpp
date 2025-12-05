#include <iostream>
#include <vector>
#include <string>
#include <sodium.h>

#include "../utils/logging.hpp"
#include "../auth/AuthManager.hpp"
#include "../storage/Storage.hpp"
#include "../core/Scheduler.hpp"

std::string itemFileFor(const std::string& username) {
    return "data_" + username + ".dat";
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
        std::cout << "   Interval: " << it.interval << " days\n";
        std::cout << "   Ease: " << it.ease_factor << "\n";
        std::cout << "   Lapses: " << it.lapses << "\n";
        std::cout << "   Streak: " << it.streak << "\n";
        std::cout << "   Next review: " << it.next_review << " (UNIX)\n";
        std::cout << "   Leech: " << (it.is_leech ? "YES" : "NO") << "\n";
        std::cout << "-----------------------------\n";
    }
}

int askQuality() {
    while (true) {
        std::cout << "\nChoose difficulty:\n";
        std::cout << " 1 = AGAIN (Failed)\n";
        std::cout << " 2 = HARD\n";
        std::cout << " 3 = GOOD\n";
        std::cout << " 4 = EASY\n";
        std::cout << "> ";

        int q;
        if (std::cin >> q && q >= 1 && q <= 4) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return q;
        }

        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid input. Please choose 1–4.\n";
    }
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

    Log::init();

    AuthManager auth;
    Scheduler scheduler;
    std::vector<Item> items;
    User* current = nullptr;

    // LOGIN LOOP
    while (!current) {
        std::cout << "\n===== LOGIN MENU =====\n";
        std::cout << "1. Login\n";
        std::cout << "2. Signup\n";
        std::cout << "3. Exit\n> ";

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(1024, '\n');
            continue;
        }
        std::cin.ignore();

        if (choice == 1) {
            std::string username, password;
            std::cout << "Username: ";
            std::getline(std::cin, username);
            std::cout << "Password: ";
            std::getline(std::cin, password);

            if (auth.login(username, password)) {
                current = auth.getCurrentUser();
                const auto& key = auth.getSessionKey();

                if (!Storage::loadItems(items, itemFileFor(current->username), key)) {
                    std::cout << "Warning: Could not decrypt item file.\n";
                }
                else {
                    std::cout << "Items loaded.\n";
                }
            }
            else {
                std::cout << "Invalid username or password.\n";
            }
        }
        else if (choice == 2) {
            std::string username, password;
            std::cout << "Choose username: ";
            std::getline(std::cin, username);
            std::cout << "Choose password: ";
            std::getline(std::cin, password);

            if (auth.signup(username, password)) {
                std::cout << "Signup successful. Please login.\n";
            }
            else {
                std::cout << "Signup failed (username may exist).\n";
            }
        }
        else if (choice == 3) {
            return 0;
        }
    }

    // MAIN LOOP
    while (true) {
        std::cout << "\n===== MAIN MENU =====\n";
        std::cout << "User: " << current->username << "\n";
        std::cout << "1. Add Item\n";
        std::cout << "2. Review Due Items\n";
        std::cout << "3. List All Items\n";
        std::cout << "4. Save & Exit\n> ";

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(1024, '\n');
            continue;
        }
        std::cin.ignore();

        if (choice == 1) {
            std::string title, content;

            std::cout << "Enter title: ";
            std::getline(std::cin, title);

            std::cout << "Enter content (optional): ";
            std::getline(std::cin, content);

            if (title.empty()) {
                std::cout << "Title cannot be empty.\n";
                continue;
            }

            items.emplace_back(title, content);
            std::cout << "Item created.\n";
        }

        // REVIEW
        else if (choice == 2) {
            auto due = scheduler.getDueItems(items);

            if (due.empty()) {
                std::cout << "No items due.\n";
                continue;
            }

            for (auto* item : due) {
                std::cout << "\n===== REVIEW ITEM =====\n";
                std::cout << "Title:   " << item->title << "\n";
                std::cout << "Content: " << item->content << "\n\n";
                std::cout << "Interval: " << item->interval << " days\n";
                std::cout << "Ease:     " << item->ease_factor << "\n";
                std::cout << "Lapses:   " << item->lapses << "\n";
                std::cout << "Streak:   " << item->streak << "\n";
                std::cout << "Next Review (UNIX): " << item->next_review << "\n";

                int q = askQuality();
                scheduler.review(*item, static_cast<ReviewQuality>(q));

                // Update item stats (high-level)
                if (q == 1) {
                    item->streak = 0;
                }
                else {
                    item->review_count++;
                    item->streak++;
                }

                std::cout << "Item updated.\n";
            }
        }

        else if (choice == 3) {
            listAllItems(items);
        }

        else if (choice == 4) {
            const auto& key = auth.getSessionKey();

            if (!Storage::saveItems(items, itemFileFor(current->username), key)) {
                std::cout << "Failed to save encrypted data.\n";
            }
            else {
                std::cout << "Items saved.\n";
            }

            auth.save();
            auth.logout();
            std::cout << "Goodbye!\n";
            break;
        }
    }

    return 0;
}
