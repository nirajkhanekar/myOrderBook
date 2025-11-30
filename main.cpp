#include "OrderBook.h"
#include <iostream>
// --------------------------- Demo / Quick Tests ----------------------------

using namespace ob;
void print_summary(const OrderBook &ob) {
    cout << "Top Bid: ";
    if (auto t = ob.top_price(Side::Bid)) cout << *t;
    else cout << "(none)";
    cout << " | Top Ask: ";
    if (auto t = ob.top_price(Side::Ask)) cout << *t;
    else cout << "(none)";
    cout << " | Crossed? " << (ob.is_crossed() ? "YES" : "NO") << "\n";
}

void print_side(const OrderBook &ob, Side s) {
    cout << (s == Side::Bid ? "Bids:\n" : "Asks:\n");
    auto prices = ob.price_levels(s);
    for (double p : prices) {
        cout << "  Price " << p << " -> ";
        auto orders = ob.orders_at(s, p);
        for (auto &o : orders) {
            cout << "[id=" << o->id << ", q=" << o->quantity << ", lu=" << time_to_string(o->last_update_time)
                 << "] ";
        }
        cout << "\n";
    }
}

int main() {
    ob::OrderBook ob;

    // Build Book (worked example from PDF)
    // Add 1: buy 400 @ 50
    ob.add_order("1", Side::Bid, 50.0, 400);
    // Add 2: buy 300 @ 50
    ob.add_order("2", Side::Bid, 50.0, 300);
    // Add 3: sell 400 @ 55
    ob.add_order("3", Side::Ask, 55.0, 400);
    // Add 4: buy 100 @ 51
    ob.add_order("4", Side::Bid, 51.0, 100);
    // Add 5: sell 200 @ 56
    ob.add_order("5", Side::Ask, 56.0, 200);

    cout << "Initial Book (Book 1):\n";
    print_side(ob, Side::Bid);
    print_side(ob, Side::Ask);
    print_summary(ob);
    cout << "----\n";


    // Cancel Order 1
    ob.remove_order("1");
    cout << "After cancelling Order 1:\n";
    print_side(ob, Side::Bid);
    print_side(ob, Side::Ask);
    cout << "----\n";

    // Re-add order 1 then amend to qty 500 (demonstrate priority change when amended)
    ob.add_order("1", Side::Bid, 50.0, 400);
    // Amend order 1 to 500 qty (price same but quantity increased -> priority moves to back)
    ob.amend_order("1", optional<double>{50.0}, optional<uint64_t>{500});
    cout << "After amending Order 1 to qty 500 (same price, increased qty => loses priority):\n";
    print_side(ob, Side::Bid);
    cout << "----\n";

    // Amend order 2: price same, quantity decreased -> should keep priority (order 2 stays above 1)
    ob.amend_order("2", optional<double>{50.0}, optional<uint64_t>{200});
    cout << "After amending Order 2 to qty 200 (same price, decreased qty => keep priority):\n";
    print_side(ob, Side::Bid);
    cout << "----\n";

    // Fully match example: receive Order 6 to sell 150 @ 50 (matches bids)
    cout << "Simulating fill: Order 6 sells 150 @ 50 (manual simulation)\n";
    // Remove order 4
    ob.remove_order("4");
    // Reduce order 1 from 500 to 450
    ob.amend_order("2", optional<double>{50.0}, optional<uint64_t>{150});
    print_side(ob, Side::Bid);
    print_side(ob, Side::Ask);
    cout << "----\n";

    // Iterate orders created before/after a time
    auto tpoint = chrono::system_clock::now();
    auto created_after = ob.orders_created_after(tpoint); // probably empty
    cout << "Orders created after now: " << created_after.size() << "\n";

    // Example: query order by id
    if (auto o = ob.get_order("3")) {
        cout << "Order 3 info: side=" << (*o)->side_str() << " price=" << (*o)->price
             << " qty=" << (*o)->quantity << " created=" << time_to_string((*o)->creation_time) << "\n";
    }
    
    cout << "Demo complete.\n";
    return 0;
}
