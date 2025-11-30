#include "OrderBook.h"
#include <chrono>
namespace ob
{

bool OrderBook::add_order(const string &id, Side side, double price, uint64_t qty, TimePoint t)
{
    if (orders_by_id.count(id)) 
        return false; // id must be unique

    // Insert into correct side map
    //auto &pl_map = (side == Side::Bid) ? bid_book : ask_book;
    auto order = std::make_shared<Order>(id, side, price, qty, t);
    if(side == Side::Bid)
    {
        auto pit = bid_book.find(price);
        if(pit == bid_book.end())
            pit = bid_book.emplace(price, PriceLevel(price)).first;
        
        // push_back (new order arrives now -> last_update_time is current)  
        pit->second.orders.push_back(order);
        
        //store lookup: pointer to price level (map key) and iterator to list element
        auto list_it = prev(pit->second.orders.end());
        orders_by_id[id] = {side, price, list_it};
        order->last_txn = {TxnType::Add, t};
    }
    else
    {
        auto pit = ask_book.find(price);
        if(pit == ask_book.end())
            pit = ask_book.emplace(price, PriceLevel(price)).first;
        
        // push_back (new order arrives now -> last_update_time is current)  
        pit->second.orders.push_back(order);
        
        //store lookup: pointer to price level (map key) and iterator to list element
        auto list_it = prev(pit->second.orders.end());
        orders_by_id[id] = {side, price, list_it};
        order->last_txn = {TxnType::Add, t}; 
    }
    return true;    
}

bool OrderBook::remove_order(const string &id, TimePoint t)
{
    auto info_it = orders_by_id.find(id);
    if (info_it == orders_by_id.end()) return false;

    auto &info = info_it->second;
    auto &side = info.side;
    auto &price = info.price;

    //auto &pl_map = (side == Side::Bid) ? bid_book : ask_book;
    auto exe = [&, this](auto &pl_map)
    {
        auto pl_it = pl_map.find(price);
        if (pl_it == pl_map.end()) 
            return false; // shouldn't happen

        // erase order from list
        pl_it->second.orders.erase(info.list_it);

        // if price level empty, remove it
        if (pl_it->second.orders.empty()) 
            pl_map.erase(pl_it);

        orders_by_id.erase(info_it);
        return true;
    };
    
    if(side == Side::Bid)
      return exe(bid_book);
    else
      return exe(ask_book);    
}

// Amend order: price and/or quantity. Behavior:
// - If price changes -> order gets reinserted at new price level and its last_update_time becomes t.
// - If price same and quantity increases -> update quantity and last_update_time = t (priority changes).
// - If price same and quantity decreases -> update quantity but KEEP priority (do not modify last_update_time nor re-order).
// Returns true if amend succeeded.
bool OrderBook::amend_order(const string &id, optional<double> new_price,
                     optional<uint64_t> new_qty, TimePoint t)
{
    auto info_it = orders_by_id.find(id);
    if (info_it == orders_by_id.end()) return false;
    auto info = info_it->second;
    auto o_shared = *info.list_it;
    if (!o_shared) 
        return false;

    double old_price = info.price;
    uint64_t old_qty = o_shared->quantity;
    Side side = info.side;

    bool price_changed = new_price.has_value() && new_price.value() != old_price;
    bool qty_changed = new_qty.has_value() && new_qty.value() != old_qty;

    // If only quantity decreased and price unchanged -> maintain priority
    bool keep_priority = (!price_changed) && qty_changed &&
                         new_qty.value() < old_qty;

    if (price_changed) 
    {
        // Remove from old price level
        // auto &pl_map_old = (side == Side::Bid) ? bid_book : ask_book;
        auto exe = [&, this](auto &pl_map)
        {
            auto pl_it_old = pl_map.find(old_price);
            if (pl_it_old != pl_map.end()) {
                pl_it_old->second.orders.erase(info.list_it);
                if (pl_it_old->second.orders.empty()) 
                    pl_map.erase(pl_it_old);
            }

            // Update order fields
            o_shared->price = new_price.value();
            if (new_qty.has_value()) o_shared->quantity = new_qty.value();
            o_shared->last_update_time = t;
            o_shared->last_txn = {TxnType::Amend, t};

            // Insert into new price level at the back (new update -> later update time -> lower priority)
            //auto &pl_map_new = (side == Side::Bid) ? bid_book : ask_book;
            auto pl_it_new = pl_map.find(o_shared->price);
            if (pl_it_new == pl_map.end()) 
            {
                PriceLevel pl(o_shared->price);
                auto inserted = pl_map.emplace(o_shared->price, move(pl)).first;
                pl_it_new = inserted;
            }
            pl_it_new->second.orders.push_back(o_shared);
            orders_by_id[id] = {side, o_shared->price, prev(pl_it_new->second.orders.end())};
        };
        
        if(side == Side::Bid)
            exe(bid_book);
        else
            exe(ask_book);

        return true;
    } 
    else 
    {
        // price same
        if (!qty_changed) return false; // nothing to do

        if (keep_priority) 
        {
            // reduce qty but keep priority; do not touch last_update_time or ordering
            o_shared->quantity = new_qty.value();
            o_shared->last_txn = {TxnType::Amend, t};
            // last_update_time unchanged
            return true;
        } 
        else 
        {
            // Either quantity increased or some other update that should change priority
            // We update last_update_time and move order to the back of its price level (later update time => lower priority)
            //auto &pl_map = (side == Side::Bid) ? bid_book : ask_book;
            auto exeCp = [&, this](auto &pl_map)
            {
                auto pl_it = pl_map.find(old_price);
                if (pl_it == pl_map.end()) return false; // should not happen

                // erase from current position and push_back (so it becomes later in ordering)
                pl_it->second.orders.erase(info.list_it);
                o_shared->quantity = new_qty.value();
                o_shared->last_update_time = t;
                o_shared->last_txn = {TxnType::Amend, t};
                pl_it->second.orders.push_back(o_shared);
                orders_by_id[id] = {side, old_price, prev(pl_it->second.orders.end())};
                return true;
            };
            
            if(side == Side::Bid)
                return exeCp(bid_book);
            else
                return exeCp(ask_book);
            
        }
    } 
}

// Query whether book is crossed: top ask price <= top bid price
bool OrderBook::is_crossed() const
{
    auto top_bid = top_price(Side::Bid);
    auto top_ask = top_price(Side::Ask);
    if (!top_bid.has_value() || !top_ask.has_value()) return false;
    return top_ask.value() <= top_bid.value();
}
// Top price for a side (empty => nullopt)
std::optional<double> OrderBook::top_price(Side s) const
{
    if (s == Side::Bid) 
    {
        if (bid_book.empty()) return {};
        return bid_book.begin()->first; // desc comparator -> begin() is highest
    } 
    else 
    {
        if (ask_book.empty()) return {};
        return ask_book.begin()->first; // asc map -> begin() lowest
    }
}

// Bottom price for a side (empty => nullopt)
std::optional<double> OrderBook::bottom_price(Side s) const
{
    if (s == Side::Bid) 
    {
        if (bid_book.empty()) return {};
        return bid_book.rbegin()->first;
    } 
    else
    {
        if (ask_book.empty()) return {};
        return ask_book.rbegin()->first;
    }   
}
// Number of price levels on a side
size_t OrderBook::num_price_levels(Side s) const
{
    if(s == Side::Bid) 
        return bid_book.size();
    else 
        return ask_book.size();    
}

// Iterate price levels: returns vector of prices in priority order
std::vector<double> OrderBook::price_levels(Side s) const
{
    vector<double> res;
    if (s == Side::Bid) 
    {
        for (const auto &kv : bid_book) res.push_back(kv.first);
    } 
    else 
    {
        for (const auto &kv : ask_book) res.push_back(kv.first);
    }
    return res; 
}

// Number of orders at a price level
size_t OrderBook::num_orders_at(Side s, double price) const
{
    auto exe = [&, this](auto &pl_map)
    {
        auto it = pl_map.find(price);
        if (it == pl_map.end()) 
            return size_t(0);
        return it->second.order_count();
    };
    if(s == Side::Bid)
        return exe(bid_book);
    else
       return exe(ask_book);
}
// Iterate orders at a price level by priority (earliest update time first) -> returns vector of shared_ptr<Order>
std::vector<shared_ptr<Order>> OrderBook::orders_at(Side s, double price) const
{
    vector<shared_ptr<Order>> res;
    auto exe = [&, this](auto &pl_map)
    {
        auto it = pl_map.find(price);
        if (it == pl_map.end()) 
          return;
        for (auto &o : it->second.orders) 
            res.push_back(o);
    };
    if( s == Side::Bid)
        exe(bid_book);
    else
        exe(ask_book);
        
    return res;
}
// Number of orders across all prices on a side
size_t OrderBook::num_orders_on_side(Side s) const
{
    size_t cnt = 0;
    if (s == Side::Bid) {
        for (auto &kv : bid_book) cnt += kv.second.order_count();
    } else {
        for (auto &kv : ask_book) cnt += kv.second.order_count();
    }
    return cnt;   
}
// Iterate orders across all prices on a side by priority (price priority then update time)
std::vector<shared_ptr<Order>> OrderBook::orders_on_side(Side s) const
{
    vector<shared_ptr<Order>> res;
    if (s == Side::Bid) {
        for (auto &kv : bid_book)
            for (auto &o : kv.second.orders) res.push_back(o);
    } else {
        for (auto &kv : ask_book)
            for (auto &o : kv.second.orders) res.push_back(o);
    }
    return res;  
}
// Get order info by id
std::optional<shared_ptr<Order>> OrderBook::get_order(const string &id) const
{
    auto it = orders_by_id.find(id);
    if (it == orders_by_id.end()) 
     return {};
    return *it->second.list_it;   
}
// Last transaction on order id (if exists)
std::optional<Transaction> OrderBook::last_transaction(const string &id) const
{
    auto o = get_order(id);
    if (o.has_value()) return (*o)->last_txn;
    // If removed, we do not keep history except possibly in the removed shared_ptr that got destroyed.
    return {};
}
// Iterate orders created before/after given time (across entire book)
std::vector<shared_ptr<Order>> OrderBook::orders_created_before(TimePoint t) const
{
   vector<shared_ptr<Order>> res;
    for (const auto &kv : orders_by_id) {
        if (auto o = *kv.second.list_it)
            if (o->creation_time < t) res.push_back(o);
    }
    return res; 
}
std::vector<shared_ptr<Order>> OrderBook::orders_created_after(TimePoint t) const
{
    vector<shared_ptr<Order>> res;
    for (const auto &kv : orders_by_id) {
        if (auto o = *kv.second.list_it)
            if (o->creation_time > t) res.push_back(o);
    }
    return res;
}
std::vector<shared_ptr<Order>> OrderBook::orders_updated_before(TimePoint t) const
{
    vector<shared_ptr<Order>> res;
    for (const auto &kv : orders_by_id) {
        if (auto o = *kv.second.list_it)
            if (o->last_update_time < t) res.push_back(o);
    }
    return res; 
}
std::vector<shared_ptr<Order>> OrderBook::orders_updated_after(TimePoint t) const
{
    vector<shared_ptr<Order>> res;
    for (const auto &kv : orders_by_id) {
        if (auto o = *kv.second.list_it)
            if (o->last_update_time > t) res.push_back(o);
    }
    return res;
}

}