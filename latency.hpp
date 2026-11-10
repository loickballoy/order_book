// latency.hpp — mesure de latence par opération : p50, p99, p99.9, max.
// En-tête seul, C++17, aucune dépendance.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

// Horloge monotone en nanosecondes.
inline uint64_t now_ns() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

// Coût d'une paire d'appels à now_ns() (médiane), à afficher à côté des résultats.
inline uint64_t timer_overhead_ns(int samples = 100000) {
    std::vector<uint64_t> d;
    d.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        uint64_t t0 = now_ns();
        uint64_t t1 = now_ns();
        d.push_back(t1 - t0);
    }
    std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
    return d[d.size() / 2];
}

class LatencyRecorder {
public:
    // Réserver la capacité AVANT la boucle mesurée : aucune allocation pendant la mesure.
    explicit LatencyRecorder(const char* name, size_t expected = 0) : name_(name) {
        samples_.reserve(expected);
    }

    void add(uint64_t ns) { samples_.push_back(ns); sorted_ = false; }

    // Ignore les n premiers échantillons (échauffement des caches).
    void drop_first(size_t n) {
        samples_.erase(samples_.begin(),
                       samples_.begin() + static_cast<std::ptrdiff_t>(std::min(n, samples_.size())));
    }

    size_t count() const { return samples_.size(); }

    // Percentile par rang le plus proche : la plus petite valeur telle qu'au moins
    // p % des échantillons lui sont inférieurs ou égaux.
    uint64_t percentile(double p) {
        if (samples_.empty()) return 0;
        sort_once();
        // Le -1e-9 évite qu'une erreur d'arrondi (99.9 / 100 * 1000 = 999.0000000000001)
        // ne décale le rang d'un cran.
        size_t rank = static_cast<size_t>(std::ceil(p * samples_.size() / 100.0 - 1e-9));
        if (rank == 0) rank = 1;
        return samples_[rank - 1];
    }

    void report() {
        if (samples_.empty()) {
            std::printf("%-18s : aucun échantillon\n", name_);
            return;
        }
        std::printf("%-18s : n=%-7zu p50 %5llu ns  p99 %6llu ns  p99.9 %7llu ns  max %8llu ns\n",
                    name_, samples_.size(),
                    (unsigned long long)percentile(50.0),
                    (unsigned long long)percentile(99.0),
                    (unsigned long long)percentile(99.9),
                    (unsigned long long)percentile(100.0));
    }

private:
    void sort_once() {
        if (!sorted_) {
            std::sort(samples_.begin(), samples_.end());
            sorted_ = true;
        }
    }

    const char* name_;
    std::vector<uint64_t> samples_;
    bool sorted_ = false;
};