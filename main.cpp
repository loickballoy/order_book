#include <iostream>
#include "order_book.cpp"
#include <chrono>
#include <cstdlib>
#include <random>

int main(int argc, char** argv){
    int numOrders = (argc > 1) ? std::atoi(argv[1]) : 1000;
    bool verbose = (numOrders <= 20);

    std::mt19937 rng(42);

    std::uniform_int_distribution<int> priceDist(95, 105);
    std::uniform_int_distribution<int> qtyDist(1, 50);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> typeDist(0, 1);

    OrderBook book;

    std::vector<Order> generatedOrders;
    generatedOrders.reserve(numOrders);
    for (int i = 0; i < numOrders; ++i){
        OrderType type = static_cast<OrderType>(typeDist(rng));
        Side side = static_cast<Side>(sideDist(rng));
        double price = static_cast<double>(priceDist(rng));
        int qty = qtyDist(rng);
        generatedOrders.emplace_back(i, type, side, price, qty);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& order : generatedOrders){
        book.addOrder(order);
    }
    if (verbose) {
        std::cout << "\n--- Etat initial du book ---\n";
        book.printOrders();
    }
    auto t1= std::chrono::high_resolution_clock::now();


    book.matchOrders();
    auto t2 = std::chrono::high_resolution_clock::now();

    if (verbose) {
        std::cout << "\n--- Etat final du book ---\n";
        book.printOrders();
    }

    auto insertUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    auto matchUs = std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count();

    std::cout << "\n--- Benchmark (" << numOrders << " orders) ---\n";
    std::cout << "Insertion : " << insertUs << " us"
              << " (" << (double)insertUs / numOrders << " us/order)\n";
    std::cout << "Matching  : " << matchUs << " us\n";
    std::cout << "Total     : " << (insertUs + matchUs) << " us\n";

    return 0;
}