#include <iostream>
#include <vector>
#include <string>
#include <sodium.h>

#include "../auth/AuthManager.hpp"
#include "../storage/Storage.hpp"
#include "../core/Scheduler.hpp"

// Generate safe per-user data file
std::string itemFileFor(const std::string& username) {
    return "items_" + username + ".txt";
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
    //
    // ===== Initialize libsodium =====
    //
    if (sodium_init() < 0) {
        std::cerr << "Fatal error: could not initialize libsodium.\n";
        return 1;
    }

    //
    // ===== Setup managers =====
    //
    AuthManager auth("users.txt");   // loads users internally
    Scheduler scheduler;

    std::vector<Item> items;
    User* current = nullptr;

    //
    // ===== LOGIN / SIGNUP LOOP =====
    //
    while (!current) {
        std::cout << "\n===== AUTH MENU =====\n";
        std::cout << "1. Login\n";
        std::cout << "2. Signup\n";
        std::cout << "3. Exit\n> ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1) {
            std::string username, password;

            std::cout << "Username: ";
            std::getline(std::cin, username);

            std::cout << "Password: ";
            std::getline(std::cin, password);

            if (auth.login(username, password)) {
                std::cout << "\nLogin successful.\n";
                current = auth.getCurrentUser();

                // Load per-user items
                Storage::loadItems(items, itemFileFor(username));
            }
            else {
                std::cout << "\nInvalid username or password.\n";
            }
        }
        else if (choice == 2) {
            std::string username, password;

            std::cout << "Choose username: ";
            std::getline(std::cin, username);

            std::cout << "Choose password: ";
            std::getline(std::cin, password);

            if (username.empty() || password.empty()) {
                std::cout << "Username and password cannot be empty.\n";
                continue;
            }

            if (auth.signup(username, password)) {
                std::cout << "Signup successful. You may now log in.\n";
            }
            else {
                std::cout << "That username is already taken.\n";
            }
        }
        else if (choice == 3) {
            return 0;
        }
        else {
            std::cout << "Invalid option.\n";
        }
    }

    //
    // ===== MAIN APPLICATION LOOP =====
    //
    while (true) {
        std::cout << "\n===== MAIN MENU =====\n";
        std::cout << "Logged in as: " << current->username << "\n";
        std::cout << "1. Add Item\n";
        std::cout << "2. Review Due Items\n";
        std::cout << "3. List All Items\n";
        std::cout << "4. Save & Exit\n> ";

        int choice;
        std::cin >> choice;
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
            std::cout << "Added.\n";
        }
        else if (choice == 2) {
            auto due = scheduler.getDueItems(items);

            if (due.empty()) {
                std::cout << "No items due for review.\n";
                continue;
            }

            for (auto* item : due) {
                std::cout << "\nReview: " << item->title << "\n";
                std::cout << "Quality? (1=Again, 2=Hard, 3=Good, 4=Easy): ";

                int q;
                std::cin >> q;
                std::cin.ignore();

                if (q < 1 || q > 4) {
                    std::cout << "Invalid response. Skipped.\n";
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
            std::string file = itemFileFor(current->username);

            Storage::saveItems(items, file);   // save items
            auth.save();                       // save users

            std::cout << "All data saved. Goodbye!\n";
            break;
        }
        else {
            std::cout << "Invalid option.\n";
        }
    }

    return 0;
}
