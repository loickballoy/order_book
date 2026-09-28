#include "order_book.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include <unistd.h>

static void onTimeout(int){
    const char msg[] = "\n[TIMEOUT] infinite loop\n";
    ssize_t r = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)r;
    _exit(2);
}

static void armWatchdog(unsigned seconds){
    std::signal(SIGALRM, onTimeout);
    alarm(seconds);
}

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) \
    do { \
        ++g_checks; \
        if (!(cond)) { \
            ++g_failures; \
            std::cerr << " FAIL (ligne " << __LINE__ << ") : " #cond << "\n"; \
        } \
    } while (0)

static void title(const char *t) { std::cout << t << std::endl;}
static void add(OrderBook& b, int id, OrderType t, Side s, double px, int qty) {
    b.addOrder(Order(id, t, s, px, qty));
}

// Un sell au-dessus du meilleur bid ne doit ni trader ni bloquer.
static void testNonCrossingSell() {
    title("[1] sell au-dessus du meilleur bid");
    OrderBook b;
    add(b, 1, GTC, BUY, 100.0, 10);
    add(b, 2, GTC, SELL, 105.0, 5);
    CHECK(b.getTradeCount() == 0);
    CHECK(b.getRestingQty(BUY) == 10);
    CHECK(b.getRestingQty(SELL) == 5);
    CHECK(b.getIndexSize() == 2);
}
 
// Cancel d'un ID inconnu, double cancel, prix non entier, pas de niveau fantôme.
static void testCancel() {
    title("[2] cancel : inconnu, double, prix non entier");
    OrderBook b;
    add(b, 1, GTC, SELL, 100.5, 5);
    b.cancelOrder(999);                    // ID inconnu
    b.cancelOrder(1);
    b.cancelOrder(1);                      // déjà annulé
    CHECK(b.getRestingQty(SELL) == 0);
    CHECK(b.getIndexSize() == 0);
    CHECK(b.getRestingOrderCount() == 0);
    add(b, 2, GTC, BUY, 101.0, 5);         // ne doit rien croiser
    CHECK(b.getTradeCount() == 0);
    CHECK(b.getRestingQtyOf(2) == 5);
}
 
// Plusieurs niveaux, remplissage partiel, priorité temporelle, arrêt au prix limite.
static void testMultiLevelAndFifo() {
    title("[3] multi-niveaux + FIFO + reliquat");
    OrderBook b;
    add(b, 1, GTC, SELL, 100.0, 5);
    add(b, 2, GTC, SELL, 100.0, 5);        // même niveau, arrivé après l'ordre 1
    add(b, 3, GTC, SELL, 101.0, 10);
    add(b, 4, GTC, SELL, 102.0, 10);
 
    add(b, 5, GTC, BUY, 101.0, 7);         // 5 (ordre 1) + 2 (ordre 2)
    CHECK(b.getTradedQty() == 7);
    CHECK(b.getTradeCount() == 2);
    CHECK(b.getRestingQtyOf(1) == -1);     // ordre 1 entièrement rempli
    CHECK(b.getRestingQtyOf(2) == 3);      // ordre 2 partiellement rempli (FIFO)
    CHECK(b.getRestingQtyOf(5) == -1);     // l'acheteur est totalement exécuté
 
    add(b, 6, GTC, BUY, 101.0, 20);        // 3 + 10, puis 102 > 101 : stop
    CHECK(b.getTradedQty() == 20);
    CHECK(b.getTradeCount() == 4);
    CHECK(b.getRestingQtyOf(2) == -1);
    CHECK(b.getRestingQtyOf(3) == -1);
    CHECK(b.getRestingQtyOf(4) == 10);     // niveau 102 intact
    CHECK(b.getRestingQtyOf(6) == 7);      // reliquat posé dans le book
    CHECK(b.getRestingQty(BUY) == 7);
    CHECK(b.getRestingQty(SELL) == 10);
    CHECK(b.getRestingOrderCount() == 2);
    CHECK(b.getIndexSize() == 2);
}
 
// IOC : exécute ce qu'il peut, le reliquat n'entre jamais dans le book.
static void testIoc() {
    title("[4] IOC : reliquat jamais posé");
    OrderBook b;
    add(b, 1, GTC, SELL, 100.0, 5);
    add(b, 2, IOC, BUY, 100.0, 8);         // 5 exécutés, 3 abandonnés
    CHECK(b.getTradedQty() == 5);
    CHECK(b.getRestingQtyOf(2) == -1);
    CHECK(b.getRestingQty(BUY) == 0);
    CHECK(b.getRestingQty(SELL) == 0);
    add(b, 3, IOC, BUY, 90.0, 5);          // ne croise rien : ne doit pas rester
    CHECK(b.getRestingQty(BUY) == 0);
    CHECK(b.getIndexSize() == 0);
}
 
// Symétrie côté sell : un sell qui traverse plusieurs bids.
static void testSellSide() {
    title("[5] sell qui traverse plusieurs bids");
    OrderBook b;
    add(b, 1, GTC, BUY, 100.0, 5);
    add(b, 2, GTC, BUY, 99.0, 5);
    add(b, 3, GTC, BUY, 98.0, 5);
    add(b, 4, GTC, SELL, 99.0, 8);         // 5 @100 puis 3 @99, stop avant 98
    CHECK(b.getTradedQty() == 8);
    CHECK(b.getRestingQtyOf(1) == -1);
    CHECK(b.getRestingQtyOf(2) == 2);
    CHECK(b.getRestingQtyOf(3) == 5);
    CHECK(b.getRestingQtyOf(4) == -1);
}
 
static int runUnitTests() {
    armWatchdog(10);
    testNonCrossingSell();
    testCancel();
    testMultiLevelAndFifo();
    testIoc();
    testSellSide();
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " vérifications OK\n";
    return g_failures == 0 ? 0 : 1;
}
 
// ---------------------------------------------------------------------------
// Run aléatoire : benchmark + invariants de conservation
// ---------------------------------------------------------------------------
//
// Pour chaque côté :
//   soumis = échangé + au repos + annulé (cancel explicite) + abandonné (IOC)
// "abandonné" n'est pas mesuré directement : c'est le résidu. Il doit valoir
// exactement 0 sans IOC, et rester >= 0 avec IOC. Un résidu négatif = quantité créée
// de nulle part.
static bool runRandom(int numOrders, int iocPercent, unsigned seed, bool verbose, 
                        int levels = 0, int aggressivePercent = 5) {
    armWatchdog(120);
 
    OrderBook book;
    book.setVerbose(verbose);
 
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> priceTicks(950, 1050);   // prix 95.0 .. 105.0, pas de 0.1
    std::uniform_int_distribution<int> qtyDist(1, 50);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::discrete_distribution<int> typeDist({(double)(100 - iocPercent), (double)iocPercent});
 
    const int half = (levels >= 2) ? levels / 2: 1;
    const int mid = half + 1000;
    std::uniform_int_distribution<int> offsetDist(1, half);
    std::uniform_int_distribution<int> pcDist(0, 99); 

    std::vector<Order> orders;
    orders.reserve(numOrders);
    long long submittedBuy = 0, submittedSell = 0;
    for (int i = 0; i < numOrders; ++i) {
        Side side = static_cast<Side>(sideDist(rng));
        double price;                  // non entier : teste la clé double
        if (levels <= 0) {
            priceTicks(rng) / 10.0;
        } else {
            bool aggressive = pcDist(rng) < aggressivePercent;
            bool below = (side == BUY) != aggressive;
            int offset = offsetDist(rng);
            price = (below ? mid - offset : mid + offset) / 10.0;
        }
        int qty = qtyDist(rng);
        OrderType type = (typeDist(rng) == 0) ? GTC : IOC;
        orders.emplace_back(i, type, side, price, qty);
        (side == BUY ? submittedBuy : submittedSell) += qty;
    }
 
    // --- Phase 1 : insertion + matching (indissociables depuis la V2) ---
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& o : orders) {
        if (verbose) {
            std::cout << "SUBMIT id=" << o.getId() << " " << typeToString(o.getType()) << " "
                      << sideToString(o.getSide()) << " " << o.getPrice() << " x" << o.getQuantity() << "\n";
        }
        book.addOrder(o);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    
    size_t depthOrders = book.getRestingOrderCount();
    size_t levelsBuy = book.getLevelCount(BUY), levelsSell = book.getLevelCount(SELL);
    // --- Phase 2 : annulations (1 ordre sur 3) ---
    // Passe non chronométrée : on note la quantité réellement au repos avant d'annuler.
    long long cancelledBuy = 0, cancelledSell = 0;
    std::vector<int> toCancel;
    for (int id = 0; id < numOrders; id += 3) {
        int q = book.getRestingQtyOf(id);
        if (q > 0) {
            (orders[id].getSide() == BUY ? cancelledBuy : cancelledSell) += q;
            toCancel.push_back(id);
        }
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    for (int id : toCancel) book.cancelOrder(id);
    auto t3 = std::chrono::high_resolution_clock::now();
 
    // Second passage : doit être sans effet (annuler deux fois ne casse rien)
    size_t indexBefore = book.getIndexSize();
    for (int id = 0; id < numOrders; id += 3) book.cancelOrder(id);
    bool idempotent = (book.getIndexSize() == indexBefore);
 
    // --- Vérifications ---
    long long traded   = book.getTradedQty();
    long long restBuy  = book.getRestingQty(BUY);
    long long restSell = book.getRestingQty(SELL);
    long long droppedBuy  = submittedBuy  - traded - restBuy  - cancelledBuy;
    long long droppedSell = submittedSell - traded - restSell - cancelledSell;
 
    bool conservation = (iocPercent == 0)
        ? (droppedBuy == 0 && droppedSell == 0)
        : (droppedBuy >= 0 && droppedSell >= 0);
    bool indexOk = (book.getIndexSize() == book.getRestingOrderCount());
 
    if (verbose) {
        std::cout << "\n--- Etat final ---\n"
                  << "Au repos : BUY " << restBuy << " / SELL " << restSell
                  << " (" << book.getRestingOrderCount() << " ordres)\n";
    }
 
    auto us = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
    };
    long long insertUs = us(t0, t1), cancelUs = us(t2, t3);
 
    std::cout << "\n--- Benchmark (" << numOrders << " ordres, " << iocPercent << "% IOC) ---\n";
    if (levels > 0)
        std::cout << "Book profond      : " << levels << " niveaux, " << aggressivePercent << "% d'ordres agressifs \n";
    std::cout << "Insertion + matching : " << insertUs << " us ("
              << (numOrders ? (double)insertUs / numOrders : 0.0) << " us/ordre)\n"
              << "Profondeur au repos  : " << depthOrders << " ordres, "
              << levelsBuy << " niveaux BUY / " << levelsSell << " niveaux SELL\n"
              << "Annulations          : " << toCancel.size() << " effectives, " << cancelUs << " us ("
              << (toCancel.empty() ? 0.0 : (double)cancelUs / toCancel.size()) << " us/annulation)\n"
              << "Trades               : " << book.getTradeCount() << " (quantité " << traded << ")\n";
 
    std::cout << "\n--- Invariants ---\n"
              << "Conservation (BUY/SELL)    : " << (conservation ? "OK" : "ECHEC")
              << "  [abandonné BUY=" << droppedBuy << ", SELL=" << droppedSell << "]\n"
              << "Index == ordres au repos   : " << (indexOk ? "OK" : "ECHEC")
              << "  [index=" << book.getIndexSize() << ", repos=" << book.getRestingOrderCount() << "]\n"
              << "Double cancel sans effet   : " << (idempotent ? "OK" : "ECHEC") << "\n";
 
    return conservation && indexOk && idempotent;
}
 
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--test") == 0) return runUnitTests();
 
    int numOrders  = (argc > 1) ? std::atoi(argv[1]) : 1000;
    int iocPercent = (argc > 2) ? std::atoi(argv[2]) : 30;
    int levels = (argc > 3) ? std::atoi(argv[3]) : 0;
    int aggressivePercent = (argc > 4) ? std::atoi(argv[4]) : 5;

    if (numOrders < 0) numOrders = 0;
    if (iocPercent < 0) iocPercent = 0;
    if (iocPercent > 100) iocPercent = 100;
    if (levels < 0) levels = 0;
    if (levels == 1) levels = 2;
    if (aggressivePercent < 0) aggressivePercent = 0;
    if (aggressivePercent > 100) aggressivePercent = 100;

    bool verbose = (numOrders <= 20);
    return runRandom(numOrders, iocPercent, 42, verbose, levels, aggressivePercent) ? 0 : 1;
}