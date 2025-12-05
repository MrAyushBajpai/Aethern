#include <iostream>
#include <vector>
#include <string>
#include <sodium.h>

#include "../auth/AuthManager.hpp"
#include "../storage/Storage.hpp"
#include "../core/Scheduler.hpp"

std::string itemFileFor(const std::string& username) {
    // uses a .dat extension and avoids '.txt'
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
        std::cout << "   Next review (UNIX): " << it.next_review << "\n";
        std::cout << "-----------------------------\n";
    }
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

    AuthManager auth; // automatically loads users from users.txt
    Scheduler scheduler;
    std::vector<Item> items;
    User* current = nullptr;

    // LOGIN / SIGNUP
    while (current == nullptr) {
        std::cout << "\n===== LOGIN MENU =====\n";
        std::cout << "1. Login\n";
        std::cout << "2. Signup\n";
        std::cout << "3. Exit\n> ";

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::string dummy;
            std::getline(std::cin, dummy);
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
                // load user items with session key
                const auto& key = auth.getSessionKey();
                if (!Storage::loadItems(items, itemFileFor(current->username), key)) {
                    std::cout << "Warning: could not load or decrypt your data file. It may not exist or the password/key is wrong.\n";
                }
                else {
                    std::cout << "Loaded your items.\n";
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

            if (username.empty() || password.empty()) {
                std::cout << "Error: username/password cannot be empty.\n";
                continue;
            }

            if (auth.signup(username, password)) {
                std::cout << "Signup successful. You can now login.\n";
            }
            else {
                std::cout << "Username already exists or signup failed.\n";
            }
        }
        else if (choice == 3) {
            return 0;
        }
    }

    // MAIN APP LOOP
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
            std::string dummy;
            std::getline(std::cin, dummy);
            continue;
        }
        std::cin.ignore();

        if (choice == 1) {
            std::string title;
            std::cout << "Enter item title: ";
            std::getline(std::cin, title);

            if (title.empty()) {
                std::cout << "Title cannot be empty.\n";
                continue;
            }

            items.emplace_back(title);
            std::cout << "Item added.\n";
        }
        else if (choice == 2) {
            auto due = scheduler.getDueItems(items);

            if (due.empty()) {
                std::cout << "No items due for review.\n";
                continue;
            }

            for (auto* item : due) {
                std::cout << "\nReviewing: " << item->title << "\n";
                std::cout << "Quality (1=Again, 2=Hard, 3=Good, 4=Easy): ";

                int q;
                if (!(std::cin >> q)) {
                    std::cin.clear();
                    std::string dummy;
                    std::getline(std::cin, dummy);
                    std::cout << "Invalid input.\n";
                    continue;
                }
                std::cin.ignore();

                if (q < 1 || q > 4) {
                    std::cout << "Invalid response—skipping.\n";
                    continue;
                }

                scheduler.review(*item, static_cast<ReviewQuality>(q));
                std::cout << "Updated.\n";
            }
        }
        else if (choice == 3) {
            listAllItems(items);
        }
        else if (choice == 4) {
            const auto& key = auth.getSessionKey();
            if (!Storage::saveItems(items, itemFileFor(current->username), key)) {
                std::cout << "Error: could not save encrypted data.\n";
            }
            else {
                std::cout << "Items saved.\n";
            }
            // save users too (not strictly necessary unless signup happened)
            auth.save();
            auth.logout();
            std::cout << "Goodbye!\n";
            break;
        }
        else {
            std::cout << "Invalid option.\n";
        }
    }

    return 0;
}
