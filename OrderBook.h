#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


using namespace std;
using TimePoint = chrono::system_clock::time_point;

static TimePoint now_tp() { return chrono::system_clock::now(); }
static string time_to_string(TimePoint t) 
{
    auto tt = chrono::system_clock::to_time_t(t);
    char buf[64];
    ctime_r(&tt, buf);
    // ctime_r appends newline; remove it
    string s(buf);
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

namespace ob
{
enum class Side { Bid, Ask };
enum class TxnType { Add, Amend, Remove };

struct Transaction {
    TxnType type;
    TimePoint time;
};

struct Order {
    string id;
    Side side;
    double price;
    uint64_t quantity;
    TimePoint creation_time;
    TimePoint last_update_time;
    Transaction last_txn;

    Order(string id_, Side side_, double price_, uint64_t qty_,
          TimePoint now = now_tp())
        : id(move(id_)),
          side(side_),
          price(price_),
          quantity(qty_),
          creation_time(now),
          last_update_time(now),
          last_txn{TxnType::Add, now} {}

    string side_str() const { return side == Side::Bid ? "Bid" : "Ask"; }
};

// A price level maintains orders in priority order (earliest last_update_time first)
struct PriceLevel {
    double price;
    list<shared_ptr<Order>> orders; // maintained in priority order by last_update_time
    explicit PriceLevel(double p) : price(p) {}
    size_t order_count() const { return orders.size(); }
    uint64_t total_quantity() const {
        uint64_t s = 0;
        for (auto &o : orders) s += o->quantity;
        return s;
    }
};

// Comparator for bid side (highest price first)
struct DescPrice {
    bool operator()(double a, double b) const { return a > b; }
};

// Comparator for ask side (lowest price first) -- std::less is default
struct AscePrice {
    bool operator()(double a, double b) const { return a < b; }
};

class OrderBook 
{
   public:
    OrderBook() = default;

    // Add an order. Assumes id unique.
    bool add_order(const string &id, Side side, double price, uint64_t qty, TimePoint t = now_tp());

    // Remove an order by id
    bool remove_order(const string &id, TimePoint t = now_tp());

    // Amend order: price and/or quantity. Behavior:
    // - If price changes -> order gets reinserted at new price level and its last_update_time becomes t.
    // - If price same and quantity increases -> update quantity and last_update_time = t (priority changes).
    // - If price same and quantity decreases -> update quantity but KEEP priority (do not modify last_update_time nor re-order).
    // Returns true if amend succeeded.
    bool amend_order(const string &id, optional<double> new_price,
                     optional<uint64_t> new_qty, TimePoint t = now_tp());

    // Query whether book is crossed: top ask price <= top bid price
    bool is_crossed() const;
    // Top price for a side (empty => nullopt)
    optional<double> top_price(Side s) const;

    // Bottom price for a side (empty => nullopt)
    optional<double> bottom_price(Side s) const;

    // Number of price levels on a side
    size_t num_price_levels(Side s) const;

    // Iterate price levels: returns vector of prices in priority order
    vector<double> price_levels(Side s) const;

    // Number of orders at a price level
    size_t num_orders_at(Side s, double price) const;

    // Iterate orders at a price level by priority (earliest update time first) -> returns vector of shared_ptr<Order>
    vector<shared_ptr<Order>> orders_at(Side s, double price) const;

    // Number of orders across all prices on a side
    size_t num_orders_on_side(Side s) const ;

    // Iterate orders across all prices on a side by priority (price priority then update time)
    vector<shared_ptr<Order>> orders_on_side(Side s) const;

    // Get order info by id
    optional<shared_ptr<Order>> get_order(const string &id) const;
    // Last transaction on order id (if exists)
    optional<Transaction> last_transaction(const string &id) const;
    // Iterate orders created before/after given time (across entire book)
    vector<shared_ptr<Order>> orders_created_before(TimePoint t) const;
    vector<shared_ptr<Order>> orders_created_after(TimePoint t) const;
    vector<shared_ptr<Order>> orders_updated_before(TimePoint t) const;
    vector<shared_ptr<Order>> orders_updated_after(TimePoint t) const;

   private:
    // Underlying containers
    // For bids: map with custom comparator for descending prices
    map<double, PriceLevel, DescPrice> bid_book;
    map<double, PriceLevel> ask_book; // ascending by default

    // Lookup info: for fast O(1) find by id and removal/reinsertion
    struct OrderLookup {
        Side side;
        double price;
        list<shared_ptr<Order>>::iterator list_it;
    };
    unordered_map<string, OrderLookup> orders_by_id;
};

} //namespace
