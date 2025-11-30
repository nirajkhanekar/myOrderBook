# Order Book — Implementation Documentation (C++17)

This project implements a simplified but fully featured **Level-2 Order Book** in **modern C++17**, including unit tests, efficient data structures, and exchange-style time/price priority rules. It supports adding, amending, removing, and querying orders with deterministic behavior and optimized lookup.

---

## 🏗️ Overview

The Order Book maintains two sides:

- **Bids** — sorted by descending price  
- **Asks** — sorted by ascending price  

Within each price level, orders are prioritized by **last update time**, ensuring FIFO time priority.

Operations supported:
- Add, amend, remove orders  
- Query price levels  
- Query orders by side, price, ID  
- Time-based queries (created/updated before/after)  
- Crossed-market detection  

---

## 🧱 Data Structures

### 📌 Price Levels

Orders at a given price are stored in a `std::list`:

- Stable iterators  
- Efficient O(1) erase and reinsert  
- Natural FIFO ordering for time priority  

### 📌 Bid & Ask Books

Two sorted maps:

- `map<double, PriceLevel, DescPrice>` for bids  
- `map<double, PriceLevel>` for asks  

Provide:
- O(log N) price-level access  
- O(1) best-price lookup via `begin()`  

### 📌 O(1) Order Lookup

Each order ID maps to:

- Side (Bid/Ask)  
- Price  
- Direct iterator pointing into the list for that price level  

This allows:
- O(1) cancel  
- O(1) amend  
- No raw pointers  
- No scanning price levels  

---

## ⚙️ Core Operations

### ➕ Add Order
- Creates price level if needed  
- Appends to list (newest → lowest priority)  
- Inserts into lookup table  

### ✏️ Amend Order
- **Price change** → remove + reinsert at new price (priority reset)  
- **Quantity increase** → moves to back (priority lost)  
- **Quantity decrease** → priority preserved  

### ❌ Remove Order
- O(1) erase using iterator  
- Removes empty price level  
- Cleans lookup  

---

## 🔎 Query Interfaces

- `top_price(side)`  
- `bottom_price(side)`  
- `price_levels(side)`  
- `orders_at(side, price)`  
- `orders_on_side(side)`  
- `get_order(id)`  
- Created/Updated before/after time queries  

Crossed market detection:

## 📊 Complexity

| Operation | Complexity |
|----------|------------|
| Add | O(log N) + O(1) |
| Remove | O(1) + O(log N) |
| Amend price | O(1) + O(log N) |
| Amend quantity | O(1) |
| Query top/bottom | O(1) |
| Lookup by ID | O(1) |

---

## 🧪 GoogleTest Suite

Full unit test coverage includes:

- Add / remove / amend  
- Price priority  
- Time priority  
- Crossed market detection  
- Time filters  
- ID lookup  
- Orders-at-level iteration  
- Edge cases

## 🚀 Build Instructions
g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
    OrderBook.cpp main.cpp \
    -o orderbook
    
g++ -std=c++17 -O2 -Wall -Wextra -pthread \
    OrderBook.cpp \
    test_orderbook.cpp \
    -lgtest -lgtest_main \
    -o orderbook_tests

### 🔧 Operation-by-Operation Complexity

| Operation | Complexity | Notes |
|----------|------------|-------|
| **Add order** | O(log Np) | Find/create price level + O(1) insert |
| **Remove order** | O(1)–O(log Np) | O(1) remove from list; O(log Np) if level becomes empty |
| **Amend price** | O(log Np) | Remove + reinsert into new price level |
| **Amend qty (increase)** | O(1) | Priority resets → move to back |
| **Amend qty (decrease)** | O(1) | Priority preserved, in-place update |
| **Top/bottom price** | O(1) | Maps store best price at `begin()` |
| **Orders at price** | O(k) | List traversal |
| **Orders on side** | O(Nside) | Iterate entire side |
| **Lookup by ID** | O(1) | Hash table |
| **Time-based queries** | O(N) | Scan all active orders |
| **Crossed detection** | O(1) | Compare `best bid` and `best ask` |


## ⚡ High-Performance Optimizations for the Order Book

This section outlines practical, targeted optimizations that significantly improve the performance of this Order Book implementation. These suggestions apply directly to the current C++17 design (maps + lists + iterator-based lookup) and focus on reducing memory overhead, improving cache locality, and lowering operation latency.

---

### 🔥 1. Replace `shared_ptr<Order>` With `unique_ptr<Order>` or Raw Pointers
The current implementation uses `std::shared_ptr<Order>`, which incurs:

- Atomic reference counting
- Allocation of a control block
- Extra indirections

**Optimized approach:**

- Use `std::unique_ptr<Order>` inside price level lists  
- Store `Order*` in the lookup table  
- Manage lifetime through list ownership

**Benefits:**
- ~40% lower overhead for adds/erases
- Zero refcounting
- Better cache locality

---

### 🔥 2. Use `boost::container::flat_map` Instead of `std::map`
Price levels are stored in `std::map`, which is:

- Node-based (cache-unfriendly)
- Pointer-dense
- Regular tree rotations are expensive

**Optimized approach:**
Use `boost::container::flat_map`.

**Benefits:**
- 3×–10× faster than `std::map` in most workloads
- Contiguous memory → great CPU cache performance
- Still maintains sorted order for best bid/ask retrieval

Works extremely well because price levels are usually sparse.

---

### 🔥 3. Preallocate and Pool Memory
Dynamic heap allocation is a major cost for high-traffic order books.

Replace:
- Heap allocation for each `Order`
- Automatic list node allocation

With:
- `boost::fast_pool_allocator`  
- A custom object pool  
- `boost::container::pmr` monotonic buffer resources

**Benefits:**
- O(1) amortized allocation
- Zero fragmentation
- Predictable latency during updates

---

### 🔥 4. Convert Order IDs From Strings to Integers
Using `std::string` as keys in the lookup table causes:

- Hashing overhead
- Memory allocations
- Slow equality checks

**Optimized approaches:**

- Map string IDs → `uint64_t`
- Maintain a tiny intern table if original IDs are required
- Use `unordered_map<uint64_t, OrderLookup>` for the main index

**Benefits:**
- Up to 10× faster order lookup
- Zero string comparisons
- Much lower memory usage
