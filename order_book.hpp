#pragma once

#include <cstddef>
#include <list>
#include <map>
#include <unordered_map>
#include <memory_resource>

enum Side {
    BUY = 0,
    SELL
};

enum OrderType {
    IOC = 0,
    GTC,
    FOK
};

const char* typeToString(OrderType type);
const char* sideToString(Side side);

class Order {
public:
    Order(int id, OrderType type, Side side, double price, int quantity)
        : id(id), type(type), side(side), price(price), quantity(quantity)
    {}

    int getId() const { return id; }
    OrderType getType() const { return type; }
    Side getSide() const { return side; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }

    void setType(OrderType t) { type = t; }
    void setSide(Side s) { side = s; }
    void setPrice(double p) { price = p; }
    void setQuantity(int q) { quantity = q; }

    void printOrder() const;

private:
    int id;
    OrderType type;
    Side side;
    double price;
    int quantity;
};

// Où se trouve un ordre au repos : de quoi le retrouver et l'effacer en O(1)
// dans sa liste, puis nettoyer son niveau de prix s'il se vide.
class OrderLocation {
public:
    OrderLocation(Side side, double price, std::list<Order>::iterator it)
        : side(side), price(price), it(it)
    {}

    Side getSide() const { return side; }
    double getPrice() const { return price; }
    std::list<Order>::iterator getIterator() const { return it; }

private:
    Side side;
    double price;
    std::list<Order>::iterator it;
};

class OrderBook {
public:
    void addOrder(Order order);
    void cancelOrder(int orderId);

    // --- Observation (tests, benchmarks) : lecture seule ---
    void setVerbose(bool v) { verbose = v; }
    long long getTradeCount() const { return tradeCount; }
    long long getTradedQty() const { return tradedQty; }
    long long getRestingQty(Side side) const;
    std::size_t getRestingOrderCount() const;
    std::size_t getIndexSize() const { return orderMap.size(); }
    std::size_t getLevelCount(Side side) const { return (side == BUY ? buyOrderMap : sellOrderMap).size(); }
    int getRestingQtyOf(int id) const;   // -1 si l'ordre n'est pas au repos

private:
    bool verbose = false;
    long long tradeCount = 0;
    long long tradedQty = 0;

    std::pmr::unsynchronized_pool_resource orderPool;
    std::pmr::unsynchronized_pool_resource sellOrderPool;
    std::pmr::unsynchronized_pool_resource buyOrderPool;

    std::pmr::unordered_map<int, OrderLocation> orderMap{&orderPool};
    std::pmr::map<double, std::list<Order>> sellOrderMap{&sellOrderPool};   // meilleur prix (le plus bas) : begin()
    std::pmr::map<double, std::list<Order>> buyOrderMap{&buyOrderPool};    // meilleur prix (le plus haut) : rbegin()

    int matchOrder(Order order, OrderType type);
    int matchIocOrder(Order order);
    int matchGtcOrder(Order order);
    int _onBuyOrder(Order order);
    int _onSellOrder(Order order);

    // Point unique des effets de bord d'un trade : compteurs + affichage éventuel.
    void recordTrade(int qty, double price);
};