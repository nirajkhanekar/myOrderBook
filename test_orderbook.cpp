#include <gtest/gtest.h>
#include "OrderBook.h"

using namespace std;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook ob;

    // Helper: fixed timestamp
    TimePoint tp(int sec = 0) {
        return chrono::system_clock::time_point(chrono::seconds(sec));
    }
};

// -----------------------------------------------------------------------------
// EMPTY BOOK BEHAVIOR
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, EmptyBookTopPricesAreNull) {
    EXPECT_FALSE(ob.top_price(Side::Bid).has_value());
    EXPECT_FALSE(ob.top_price(Side::Ask).has_value());
    EXPECT_FALSE(ob.is_crossed());
}

// -----------------------------------------------------------------------------
// ADD ORDER
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, AddOrderIncreasesCountAndCreatesPriceLevels) {
    ob.add_order("A", Side::Bid, 50, 100);

    EXPECT_EQ(ob.num_price_levels(Side::Bid), 1);
    EXPECT_EQ(ob.num_orders_at(Side::Bid, 50), 1);

    auto o = ob.get_order("A");
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ((*o)->id, "A");
    EXPECT_EQ((*o)->price, 50);
    EXPECT_EQ((*o)->quantity, 100);
}

TEST_F(OrderBookTest, AddDuplicateIDFails) {
    EXPECT_TRUE(ob.add_order("X", Side::Bid, 50, 10));
    EXPECT_FALSE(ob.add_order("X", Side::Ask, 55, 20));  // duplicate
}

// -----------------------------------------------------------------------------
// REMOVE ORDER
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, RemoveOrderRemovesCorrectEntry) {
    ob.add_order("A", Side::Ask, 55, 100);
    EXPECT_TRUE(ob.remove_order("A"));
    EXPECT_FALSE(ob.get_order("A").has_value());
    EXPECT_EQ(ob.num_orders_on_side(Side::Ask), 0);
}

TEST_F(OrderBookTest, RemovingLastOrderRemovesPriceLevel) {
    ob.add_order("A", Side::Bid, 50, 100);
    ob.remove_order("A");

    EXPECT_EQ(ob.num_price_levels(Side::Bid), 0);
}

// -----------------------------------------------------------------------------
// AMEND ORDER — PRICE CHANGES
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, AmendPriceMovesToNewPriceLevel) {
    ob.add_order("A", Side::Bid, 50, 100);

    ob.amend_order("A", 51.0, 100);

    EXPECT_EQ(ob.num_orders_at(Side::Bid, 50), 0);
    EXPECT_EQ(ob.num_orders_at(Side::Bid, 51), 1);

    auto o = ob.get_order("A");
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ((*o)->price, 51.0);
}

// -----------------------------------------------------------------------------
// AMEND ORDER — QUANTITY DECREASE (KEEP PRIORITY)
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, AmendQuantityDecreaseKeepsPriority) {
    ob.add_order("A", Side::Bid, 50, 200);
    ob.add_order("B", Side::Bid, 50, 200);

    auto before = ob.orders_at(Side::Bid, 50);
    ASSERT_EQ(before[0]->id, "A");
    ASSERT_EQ(before[1]->id, "B");

    // Decrease qty of B (no reordering)
    ob.amend_order("B", 50.0, 150);

    auto after = ob.orders_at(Side::Bid, 50);
    EXPECT_EQ(after[0]->id, "A");
    EXPECT_EQ(after[1]->id, "B");
}

// -----------------------------------------------------------------------------
// AMEND ORDER — QUANTITY INCREASE (LOSE PRIORITY)
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, AmendQuantityIncreaseMovesToBack) {
    ob.add_order("A", Side::Bid, 50, 200);
    ob.add_order("B", Side::Bid, 50, 200);

    // Increase qty of A — should move behind B
    ob.amend_order("A", 50.0, 300);

    auto after = ob.orders_at(Side::Bid, 50);
    EXPECT_EQ(after[0]->id, "B");
    EXPECT_EQ(after[1]->id, "A");
}

// -----------------------------------------------------------------------------
// TOP/BOTTOM PRICE
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, TopPriceBidsDescAsksAsc) {
    ob.add_order("A", Side::Bid, 50, 100);
    ob.add_order("B", Side::Bid, 52, 100);
    ob.add_order("C", Side::Ask, 55, 100);
    ob.add_order("D", Side::Ask, 54, 100);

    EXPECT_EQ(ob.top_price(Side::Bid), 52);
    EXPECT_EQ(ob.top_price(Side::Ask), 54);

    EXPECT_EQ(ob.bottom_price(Side::Bid), 50);
    EXPECT_EQ(ob.bottom_price(Side::Ask), 55);
}

// -----------------------------------------------------------------------------
// PRICE LEVEL LISTING
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, PriceLevelsReturnSortedPrices) {
    ob.add_order("A", Side::Bid, 50, 10);
    ob.add_order("B", Side::Bid, 52, 10);
    ob.add_order("C", Side::Bid, 51, 10);

    auto prices = ob.price_levels(Side::Bid);
    ASSERT_EQ(prices.size(), 3);

    EXPECT_EQ(prices[0], 52);
    EXPECT_EQ(prices[1], 51);
    EXPECT_EQ(prices[2], 50);
}

// -----------------------------------------------------------------------------
// ORDER-BY-ID QUERIES
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, GetOrderReturnsCorrectData) {
    ob.add_order("Z", Side::Ask, 80, 33);
    auto o = ob.get_order("Z");
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ((*o)->quantity, 33);
}

// -----------------------------------------------------------------------------
// CREATED / UPDATED TIME FILTERS
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, OrdersCreatedBeforeAfter) {
    ob.add_order("A", Side::Bid, 50, 10, tp(10));
    ob.add_order("B", Side::Bid, 51, 10, tp(20));

    EXPECT_EQ(ob.orders_created_before(tp(15)).size(), 1);
    EXPECT_EQ(ob.orders_created_after(tp(15)).size(), 1);
}

TEST_F(OrderBookTest, OrdersUpdatedBeforeAfter) {
    ob.add_order("A", Side::Bid, 50, 10, tp(10));
    ob.amend_order("A", 50.0, 20, tp(30));

    EXPECT_EQ(ob.orders_updated_before(tp(20)).size(), 0);
    EXPECT_EQ(ob.orders_updated_after(tp(20)).size(), 1);
}

// -----------------------------------------------------------------------------
// ORDER ITERATION
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, OrdersOnSideRespectsPriceThenTime) {
    ob.add_order("A", Side::Bid, 50, 10);
    ob.add_order("B", Side::Bid, 52, 10);
    ob.add_order("C", Side::Bid, 52, 10); // after B

    auto list = ob.orders_on_side(Side::Bid);

    ASSERT_EQ(list.size(), 3);
    EXPECT_EQ(list[0]->id, "B");
    EXPECT_EQ(list[1]->id, "C");
    EXPECT_EQ(list[2]->id, "A");
}

// -----------------------------------------------------------------------------
// CROSSED MARKET DETECTION
// -----------------------------------------------------------------------------
TEST_F(OrderBookTest, CrossedMarketDetection) {
    ob.add_order("A", Side::Bid, 50, 10);
    ob.add_order("B", Side::Ask, 55, 10);
    EXPECT_FALSE(ob.is_crossed());

    ob.amend_order("B", 50.0, 10);  // now 50 <= 50
    EXPECT_TRUE(ob.is_crossed());
}
