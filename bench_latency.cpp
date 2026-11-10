// bench_latency.cpp — percentiles de latence par opération (p50, p99, p99.9).
// Programme séparé : le harness existant (main.cpp) n'est pas modifié.
//
// Usage   : ./latency <ordres> <ioc%> <niveaux> <agressifs%> [runs]
// Exemple : taskset -c 2 ./latency 200000 0 50 5 5
//
// Même générateur et même graine (42) que runRandom, pour que les scénarios
// soient comparables. Le premier run sert d'échauffement et n'est pas compté ;
// on affiche la médiane, sur les runs suivants, de chaque percentile.

#include "order_book.hpp"
#include "latency.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

struct RunStats { uint64_t p50, p99, p999, max; size_t n; };

// Copie du générateur de runRandom (avec le prix toujours initialisé).
static std::vector<Order> generate(int numOrders, int iocPercent, int levels, int aggressivePercent) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> priceTicks(950, 1050);
    std::uniform_int_distribution<int> qtyDist(1, 50);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::discrete_distribution<int> typeDist({(double)(100 - iocPercent), (double)iocPercent});
    const int half = (levels >= 2) ? levels / 2 : 1;
    const int mid = half + 1000;
    std::uniform_int_distribution<int> offsetDist(1, half);
    std::uniform_int_distribution<int> pcDist(0, 99);

    std::vector<Order> orders;
    orders.reserve(numOrders);
    for (int i = 0; i < numOrders; ++i) {
        Side side = static_cast<Side>(sideDist(rng));
        double price;
        if (levels <= 0) {
            price = priceTicks(rng) / 10.0;
        } else {
            bool aggressive = pcDist(rng) < aggressivePercent;
            bool below = (side == BUY) != aggressive;
            int offset = offsetDist(rng);
            price = (below ? mid - offset : mid + offset) / 10.0;
        }
        int qty = qtyDist(rng);
        OrderType type = (typeDist(rng) == 0) ? GTC : IOC;
        orders.emplace_back(i, type, side, price, qty);
    }
    return orders;
}

static RunStats stats(LatencyRecorder& r) {
    return {r.percentile(50.0), r.percentile(99.0), r.percentile(99.9), r.percentile(100.0), r.count()};
}

static uint64_t median(std::vector<uint64_t> v) {
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

static void print(const char* name, const std::vector<RunStats>& runs) {
    std::vector<uint64_t> p50, p99, p999;
    uint64_t worst = 0;
    size_t n = 0;
    for (const auto& s : runs) {
        p50.push_back(s.p50); p99.push_back(s.p99); p999.push_back(s.p999);
        worst = std::max(worst, s.max);
        n = s.n;
    }
    std::printf("%-18s : n=%-7zu p50 %5llu ns  p99 %6llu ns  p99.9 %7llu ns  max %8llu ns\n",
                name, n, (unsigned long long)median(p50), (unsigned long long)median(p99),
                (unsigned long long)median(p999), (unsigned long long)worst);
}

int main(int argc, char** argv) {
    int numOrders  = (argc > 1) ? std::atoi(argv[1]) : 200000;
    int iocPercent = (argc > 2) ? std::atoi(argv[2]) : 0;
    int levels     = (argc > 3) ? std::atoi(argv[3]) : 50;
    int aggressive = (argc > 4) ? std::atoi(argv[4]) : 5;
    int runs       = (argc > 5) ? std::atoi(argv[5]) : 5;
    numOrders  = std::max(numOrders, 1);
    iocPercent = std::clamp(iocPercent, 0, 100);
    aggressive = std::clamp(aggressive, 0, 100);
    if (levels == 1) levels = 2;
    runs = std::max(runs, 1);

    std::vector<RunStats> passiveRuns, aggressiveRuns, cancelRuns;

    for (int r = 0; r <= runs; ++r) {                 // r == 0 : échauffement
        std::vector<Order> orders = generate(numOrders, iocPercent, levels, aggressive);
        OrderBook book;
        book.setVerbose(false);

        LatencyRecorder passive("Insertion passive", orders.size());
        LatencyRecorder aggressiveRec("Ordre agressif", orders.size());
        LatencyRecorder cancels("Annulation", orders.size() / 3 + 1);

        // Phase 1 : insertion + matching, un chrono par ordre.
        // Agressif = l'ordre a déclenché au moins un trade.
        for (auto& o : orders) {
            const auto tradesBefore = book.getTradeCount();
            const uint64_t t0 = now_ns();
            book.addOrder(o);
            const uint64_t dt = now_ns() - t0;
            (book.getTradeCount() > tradesBefore ? aggressiveRec : passive).add(dt);
        }

        // Phase 2 : mêmes annulations que le harness (1 ordre sur 3 encore au repos).
        std::vector<int> toCancel;
        for (int id = 0; id < numOrders; id += 3)
            if (book.getRestingQtyOf(id) > 0) toCancel.push_back(id);
        for (int id : toCancel) {
            const uint64_t t0 = now_ns();
            book.cancelOrder(id);
            cancels.add(now_ns() - t0);
        }

        if (r == 0) continue;
        passiveRuns.push_back(stats(passive));
        aggressiveRuns.push_back(stats(aggressiveRec));
        cancelRuns.push_back(stats(cancels));
    }

    std::printf("\n--- Latence par opération (%d ordres, %d%% IOC, %d niveaux, %d%% agressifs) ---\n",
                numOrders, iocPercent, levels, aggressive);
    std::printf("Médiane de chaque percentile sur %d runs (1 run d'échauffement non compté)\n", runs);
    print("Insertion passive", passiveRuns);
    print("Ordre agressif", aggressiveRuns);
    print("Annulation", cancelRuns);
    std::printf("Coût du chrono     : %llu ns par mesure (inclus, non soustrait)\n",
                (unsigned long long)timer_overhead_ns());
    return 0;
}