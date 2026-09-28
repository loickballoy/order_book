# Order Book & Matching Engine

A single-instrument limit order book with a matching engine, written in modern C++.
Built iteratively: every design decision is measured against a reproducible benchmark
before and after, and correctness is checked by unit scenarios and randomized invariants.
 
**Current version:** `v0.2-price-levels` (see [Version history](#version-history)).

## Features
 
- Price-time priority: best price first, then arrival order within a price level
- Order types: `GTC` (remainder rests in the book) and `IOC` (remainder is dropped);
  `FOK` is declared but **not implemented yet**
- Cancel by order ID
- Trades execute at the price of the resting order

## Build & Run
 
```bash
g++ -O2 -std=c++17 main.cpp order_book.cpp -o bench
 
./bench --test                               # 5 deterministic scenarios, 35 checks
./bench <orders> <ioc%>                      # random flow, historical price model (101 levels)
./bench <orders> <ioc%> <levels> <aggr%>     # deep-book model (see Benchmarks)g++ -O2 -std=c++17 main.cpp -o bench
./bench <order_number>   # default : 1000
```

Verbose output (submitted orders and trades) is printed automatically when `<orders> <= 20`.
 
Run under sanitizers (recommended after any change to the data structures):
 
```bash
g++ -O1 -g -std=c++17 -fsanitize=address,undefined main.cpp order_book.cpp -o bench_san
./bench_san --test && ./bench_san 20000 30 1000 10
```

## Design
 
| Structure | Type | Role |
|---|---|---|
| Sell side | `std::map<double, std::list<Order>>` | price levels, best ask = `begin()` |
| Buy side | `std::map<double, std::list<Order>>` | price levels, best bid = `rbegin()` |
| Index | `std::unordered_map<int, OrderLocation>` | order ID → (side, price, list iterator) |
 
- **Why `std::list` per level:** its iterators stay valid when other elements are inserted
  or erased, so an order can be removed in O(1) from a stored iterator without invalidating
  the index entries of the other orders.
- **Matching happens before insertion.** An incoming order is matched against the opposite
  side first; only the remaining quantity of a `GTC` order is inserted. An `IOC` remainder
  is discarded.
- **The matching loop re-reads the best level at every iteration** and never keeps an
  iterator across an erase.
- **One removal path** (`cancelOrder`) is used both for user cancels and for fully filled
  orders, so the price-level map and the index cannot get out of sync.

### Complexity
 
`N` = resting orders, `L` = distinct price levels.
 
| Operation | Time |
|---|---|
| Add an order that does not cross | O(log L) |
| Match | O(1) per trade + O(log L) per fully filled resting order |
| Cancel by ID | O(log L) (hash lookup and list erase are O(1); locating the level is O(log L)) |
| Space | O(N + L) |
 
## Correctness
 
- **Unit scenarios** (`--test`): non-crossing sell, cancel of an unknown / already cancelled
  ID and of a non-integer price, multi-level sweep with FIFO priority and partial fill,
  IOC remainder never resting, sell crossing several bids.
- **Randomized invariants**, checked after every benchmark run:
  - *Conservation*: for each side, `submitted = traded + resting + cancelled + dropped(IOC)`.
    With 0 % IOC the dropped quantity must be exactly 0.
  - *Index consistency*: the index holds exactly the orders present in the price levels.
  - *Idempotent cancel*: cancelling the same IDs twice leaves the book unchanged.

## Benchmarks (V2)

**Method.** Single thread, `-O2`, `std::chrono`, fixed seed (42) so every run replays the
same order flow. Insertion and matching are measured together (matching happens on
arrival). Cancel timing covers only orders that are actually resting.
 
**Machine.** Linux x86-64, `g++ 15.2.0`, CPU `Intel(R) Core(TM) i5-4670 CPU @ 3.40GHz`.
 
**Deep-book workload** (`./bench 200000 0 <L> 5`): 200,000 orders, 0 % IOC. Passive buys
are placed below a mid price and passive sells above it (so they do not cross); 5 % of
orders are aggressive and cross the book. About 180,000 orders remain resting, and about
19,000 trades are executed per run.
 
| Price levels (L) | µs / order | Resting orders | Levels per side | µs / cancel |
|---:|---:|---:|---:|---:|
| 100 | 0.211 | 180,153 | 50 | 0.128 |
| 1,000 | 0.278 | 180,332 | 485 | 0.179 |
| 10,000 | 0.401 | 180,200 | ~4,760 | 0.282 |
| 100,000 | 0.567 | 180,350 | ~40,400 | 0.601 |
 
Conservation, index consistency and idempotent cancel hold on every run above.
 
**Reading the numbers.**
- The cost per order grows slowly with L (×2.7 for ×1000 more levels).
- The increment per decade of L is +0.07, +0.12, +0.17 µs. A pure O(log L) term would add a
  constant amount per decade, so growth beyond that is consistent with memory effects
  (more distinct nodes touched per operation). This is an inference, **not yet profiled**
  (`perf` / `cachegrind` are on the roadmap).
- Cancels degrade faster than inserts (×4.7): each cancel touches a hash node, a list node
  and a map node scattered in memory.
**Caveats.** One run per point. Synthetic, symmetric flow around a single mid price. Not
comparable with the `v0.1-naive` numbers (different price model, and matching then ran as
a batch pass after all insertions).

## Limitations

- `FOK` is not implemented (declared in the enum only).
- Not thread-safe; single instrument.
- All three containers are node-based (`std::map`, `std::list`, `std::unordered_map`):
  one heap allocation per resting order and per price level, poor spatial locality.
- The benchmark flow is synthetic and does not model bursts, cancels of deep orders,
  or realistic price dynamics.

These are all amelioration vectors for the next implementation.