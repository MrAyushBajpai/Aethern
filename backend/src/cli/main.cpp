#include <iostream>
#include <vector>

#include "../core/Scheduler.hpp"
#include "../storage/Storage.hpp"

void listAllItems(const std::vector<Item>& items) {
    std::cout << "\n==== All Items ====\n";

    if (items.empty()) {
        std::cout << "No items stored.\n";
        return;
    }

    for (size_t i = 0; i < items.size(); i++) {
        const auto& it = items[i];
        std::cout << i + 1 << ". " << it.title << "\n";
        std::cout << "   Interval: " << it.interval << " days\n";
        std::cout << "   Ease: " << it.ease_factor << "\n";
        std::cout << "   Next review: " << it.next_review << "\n";
        std::cout << "-------------------------\n";
    }
}

int main() {
    std::vector<Item> items;
    Scheduler scheduler;

    // Load the stored items
    Storage::load(items, "data.txt");

    while (true) {
        std::cout << "\n1. Add Item\n"
            "2. Review Due Items\n"
            "3. Save & Exit\n"
            "4. List All Items\n"
            "> ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1) {
            std::string title;
            std::cout << "Enter title: ";
            std::getline(std::cin, title);

            items.emplace_back(title);
        }
        else if (choice == 2) {
            auto due = scheduler.getDueItems(items);
            if (due.empty()) {
                std::cout << "No items due.\n";
                continue;
            }

            for (auto* item : due) {
                std::cout << "\nReview: " << item->title << "\n";
                std::cout << "Quality? (1=Again, 2=Hard, 3=Good, 4=Easy): ";
                int q;
                std::cin >> q;
                scheduler.review(*item, static_cast<ReviewQuality>(q));
            }
        }
        else if (choice == 3) {
            Storage::save(items, "data.txt");
            std::cout << "Saved. Exiting...\n";
            break;
        }
        else if (choice == 4) {
            listAllItems(items);
        }
        else {
            std::cout << "Invalid choice.\n";
        }
    }

    return 0;
}
