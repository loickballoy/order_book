#include "order_book.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>

const char* typeToString(OrderType type){
    if (type == 0) return "Immediate-Or-Cancel";
    else if (type == 1) return "Good-Til'-Cancel";
    else if (type == 2) return "Fill-Or-Kill";
    return "UNKOWN TYPE";
}
const char* sideToString(Side side){
    if (side == 0) return "BUY";
    return "SELL";
}

void Order::printOrder() const {
    std::cout << "Order Id: " << id << ", Type: " << typeToString(type) << ", Side: " << sideToString(side);
    std::cout << ", Price: " << std::fixed << std::setprecision(2) << price << ", Quantity: " << quantity << std::endl;
}

void OrderBook::addOrder(Order order){
    std::list<Order>::iterator it;
    Side side = order.getSide();
    double price = order.getPrice();
    int remaingQty = 0;
    if (side == 0) { // BUY SIDE
        // Cross with sellers
        remaingQty = matchOrder(order, order.getType());

        if (remaingQty != 0){
            order.setQuantity(remaingQty);
            buyOrderMap[price].emplace_back(order);
            it = buyOrderMap[price].end();
        }
    } else { // SELL SIDE
        // Cross with buyers
        remaingQty = matchOrder(order, order.getType());

        if (remaingQty != 0){
            order.setQuantity(remaingQty);
            sellOrderMap[price].emplace_back(order);
            it = sellOrderMap[price].end();
        }
    }
    if (remaingQty == 0) return;
    --it;
    OrderLocation location(order.getSide(), order.getPrice(), it);
    orderMap.insert_or_assign(order.getId(), location);
}

long long OrderBook::getRestingQty(Side side) const {
    const auto& levels = (side == BUY) ? buyOrderMap : sellOrderMap;
    long long total = 0;
    for (const auto& level : levels) {
        for (const auto& order : level.second) {
            total += order.getQuantity();
        }
    }
    return total;
}

std::size_t OrderBook::getRestingOrderCount() const {
    std::size_t count = 0;
    for (const auto& level : buyOrderMap) count += level.second.size();
    for (const auto& level : sellOrderMap) count += level.second.size();
    return count;
}

int OrderBook::getRestingQtyOf(int id) const {
    auto it = orderMap.find(id);
    if (it == orderMap.end()) return -1;
    return it->second.getIterator()->getQuantity();
}

int OrderBook::matchOrder(Order order, OrderType type){
    if (type == 0) {// IOC 
        return matchIocOrder(order);
    } else if (type == 1) {
        return matchGtcOrder(order);
    }
    return order.getQuantity();
}
    
void OrderBook::cancelOrder(int orderId){
    auto it = orderMap.find(orderId);

    if (it == orderMap.end()) return;
        
    Side side = it->second.getSide();
    double price = it->second.getPrice();
    auto toErase = it->second.getIterator();

    if (side == BUY) {
        auto priceIt = buyOrderMap.find(price);
        priceIt->second.erase(toErase);
        if (priceIt->second.empty()){
            buyOrderMap.erase(priceIt);
        }
    }
    else {
        auto priceIt = sellOrderMap.find(price);
        priceIt->second.erase(toErase);
        if (priceIt->second.empty()){
            sellOrderMap.erase(priceIt);
        }
    }

    orderMap.erase(it);
}


int OrderBook::matchIocOrder(Order order){
    if (order.getSide() == BUY){
        _onBuyOrder(order);
    } else {
        _onSellOrder(order);
    }
    return 0;
}

int OrderBook::matchGtcOrder(Order order){
    if (order.getSide() == BUY){
        return _onBuyOrder(order);
    } else {
        return _onSellOrder(order);
    }
}


int OrderBook::_onBuyOrder(Order order){
    while (order.getQuantity() > 0 && !sellOrderMap.empty()){
        auto lowestSell = sellOrderMap.begin();
        if (order.getPrice() >= lowestSell->first) {
            Order *sellOrder = &(lowestSell->second.front());
            int orderQty = order.getQuantity(), sellOrderQty = sellOrder->getQuantity();
            int tradeQty = std::min(orderQty, sellOrderQty);
            recordTrade(tradeQty, lowestSell->first);
            order.setQuantity(orderQty - tradeQty);
            sellOrder->setQuantity(sellOrderQty - tradeQty);

            if (sellOrder->getQuantity() == 0){
                cancelOrder(lowestSell->second.front().getId());
            }
        } else {
            return order.getQuantity();
        }
    }
    return order.getQuantity();   
}

int OrderBook::_onSellOrder(Order order){
    while (order.getQuantity() > 0 && !buyOrderMap.empty()){
        auto highestBuy = buyOrderMap.rbegin();
        if (order.getPrice() <= highestBuy->first) {
            Order *buyOrder = &(highestBuy->second.front());
            int orderQty = order.getQuantity(), buyQty = buyOrder->getQuantity();
            int tradeQty = std::min(orderQty, buyQty);
            recordTrade(tradeQty, highestBuy->first);
            order.setQuantity(orderQty - tradeQty);
            buyOrder->setQuantity(buyQty - tradeQty);

            if (buyOrder->getQuantity() <= 0){
                cancelOrder(highestBuy->second.front().getId());
            }
        } else {
                return order.getQuantity();
        }
    }
    return order.getQuantity();
}

void OrderBook::recordTrade(int qty, double price) {
    ++tradeCount;
    tradedQty += qty;
    if (verbose) {
        std::cout << "Trade: " << qty << " @ " << price << std::endl;
    }
}