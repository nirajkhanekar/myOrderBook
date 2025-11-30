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

## 📈 Time & Space Complexity

This section summarizes the computational complexity and performance behavior of the Order Book implementation.

---

### ⏱️ Time Complexity Overview

The implementation uses:

- `std::map<double, PriceLevel>` — balanced tree (O(log Np))
- `std::list<std::shared_ptr<Order>>` — stable iterators, O(1) insert/erase
- `std::unordered_map<string, OrderLookup>` — O(1) ID lookup
- Direct list iterators stored per order — O(1) removal/reinsertion

Where:

- **N = total number of orders**
- **Np = number of price levels**
- **k = orders at a given price**

---

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


## ⚡ Performance Considerations

### 🧬 List-Based Priority (Time Priority)
`std::list` is used for each price level because it provides:

- Stable iterators  
- O(1) removal and reinsertion  
- Perfect match for FIFO time-based priority  

This supports realistic exchange behavior with minimal overhead.

---

### 🗂️ Map-Based Price Trees
`std::map` maintains sorted prices:

- Best bid = `bid_book.begin()`  
- Best ask = `ask_book.begin()`  
- Insert/remove price level = O(log Np)  
- Efficient for non-contiguous price distributions  

Using two separate maps (bids descending, asks ascending) simplifies logic and keeps queries O(1).

---

### 🚀 O(1) ID Lookup and Iterator Anchoring
Each order ID maps to:

- Side (Bid/Ask)  
- Price  
- Direct iterator pointing into the list  

This allows:

- **O(1) cancels**
- **O(1) modifies**
- No weak pointers  
- No tree scans  
- No full-level traversal  

---

### 🔒 Shared Pointers for Safety
`shared_ptr<Order>` is used so:

- Orders remain valid while referenced  
- Queries can safely return references  
- No dangling pointers or manual lifetime management  

The overhead is negligible relative to correctness gains.

---

### 📉 Expected Real-World Behavior

In typical order books:

- `Np` ≪ `N`
- Most modifications are quantity-only (O(1))
- Most cancels are O(1)
- Price updates are less frequent than quantity updates

Thus the implementation is optimized for:

- High-frequency quote traffic  
- Stable and predictable performance  
- Low latency under heavy update loads  
