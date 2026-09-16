#include <iostream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <string>

enum Side{
    BUY = 0,
    SELL
};

enum OrderType {
    IOC = 0,
    GTC,
    FOK
};

const char* typeToString(OrderType type){
    if (type == 0) return "Immediate Or Cancel";
    else if (type == 1) return "Good Till Cancelled";
    else if (type == 2) return "Fill Or Kill";
    return "UNKOWN TYPE";
}
const char* sideToString(Side side){
    if (side == 0) return "BUY";
    return "SELL";
}

class Order {
public:
    Order(int id, OrderType type, Side side, double price, int quantity)
    : id(id), type(type), side(side), price(price), quantity(quantity)
    {}

    int getId(){return id;}
    OrderType getType(){return type;}
    Side getSide(){return side;}
    double getPrice() {return price;}
    int getQuantity() {return quantity;}

    void setType(OrderType type){
        this->type = type; 
    }
    void setSide(Side side){
        this->side = side;
    }
    void setPrice(double price){
        this->price = price;
    }
    void setQuantity(int quantity){
        this->quantity = quantity;
    }

    void cancelOrder(){
        this->cancelled = true;
    }

    void printOrder(){
        if (this->cancelled){
            return;
        }
        std::cout << "Order ID: " << this->id << ", Type: " << typeToString(this->type) << ", Side: " << sideToString(side);
        std::cout << ", Price: " << std::fixed << std::setprecision(2) << price << ", Quantity: " << quantity << std::endl;
    }

    bool isCancelled(){return this->cancelled;}

private:
    int id;
    OrderType type;
    Side side;
    double price;
    int quantity;
    bool cancelled = false;
};

class OrderBook {
public:
    void addOrder(Order order){
        orders.push_back(order);
        orderMap.insert_or_assign(order.getId(), orders.size() - 1);
    }

/*    void cancelOrder(Order *order){
        int orderId = order->getId();
        auto it = orderMap.find(orderId);
        if (it != orderMap.end()){
            std::cout << "Canceled Order IF: " << orderId << std::endl;
            orders.erase(orders.begin() + it->second);
            orderMap.erase(orderId);

        }
        else {
            std::cout << "Order ID: " << orderId << " Not found" << std::endl;
        }
    }

    void cancelOrderByID(int orderId){
        auto it = orderMap.find(orderId);
        if (it != orderMap.end()){
            std::cout << "Canceled Order IF: " << orderId << std::endl;
            orders.erase(orders.begin() + it->second);
            orderMap.erase(orderId);
        }
        else {
            std::cout << "Order ID: " << orderId << " Not found" << std::endl;
        }       
    }
*/
    void matchOrders(){
        for (Order &order: orders){
            if (order.getSide() == BUY && !order.isCancelled()){
                for (auto &seller : orders){
                    if (seller.getSide() == SELL && !seller.isCancelled()){
                        if (priceMatch(order.getPrice(), seller.getPrice())){
                            int matchedQty = std::min(order.getQuantity(), seller.getQuantity());
                            executeOrder(&order, &seller, matchedQty);
                            if (order.getQuantity() == 0) break;
                        }
                    }
                }  
            }
        }

        for (Order &order : orders){
            if (order.getType() == IOC){
                order.cancelOrder();
            }
        }
    }

    void printOrders(){
        for (auto order : orders){
            order.printOrder();
        }
    }
private:
    std::vector<Order> orders;
    std::unordered_map<int, int> orderMap;

    std::vector<Order> getOrdersBySide(Side side){
        std::vector<Order> result;
        for (Order order : orders){
            if (order.getSide() == side) {
                result.emplace_back(order);
            }
        }
        return result;
    }

    bool priceMatch(int buyPrice, int sellPrice){
        return buyPrice >= sellPrice;
    }

    void executeOrder(Order *buy, Order *sell, int macthedQty){

/*        std::cout << "Matched Buy Order ID: " << buy->getId()
            << " with Sell Order ID: " << sell->getId()
            << " for quantity: " << macthedQty
            << " at price: " << std::fixed << std::setprecision(2) 
            << sell->getPrice() << std::endl;
*/
        buy->setQuantity(buy->getQuantity() - macthedQty);
        sell->setQuantity(sell->getQuantity() - macthedQty);

    }
};
