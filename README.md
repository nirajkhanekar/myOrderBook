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
