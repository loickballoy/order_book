# Order Book & Matching Engine

An Order Book is a data structure used in financial markets to keep track of all buy and sell orders for a particular asset, such as stocks, bonds, or cryptocurrencies. It helps market participants know the available orders and their prices, facilitating the matching of buyers with sellers. 

We will iterate on multiple versions. This README documents V1 (a naive implementation of the Order Book Object).

## Build & Run
 
```bash
g++ -O2 -std=c++17 main.cpp -o bench
./bench <order_number>   # default : 1000
```

## Arcitecture (V1)

- `Order` : represents an individual Order (id, type, side, price, quantity)
- `OrderBook` : keeps every orders in an unique `std::vector<Order>`, with an `std::unordered_map<int, int>` (order_id -> index) for direct access by ID
- **Supported Types** `GTC` (Good-Til-Cancelled, order stays in the book) and `IOC` (Immediate-Or-Cancel, is cancelled after matching). `FOK`exists as a type but is not yet tested or implemented.

## Design

This version is purposefully naive. **No adapted data structures are used**. The goal is to get a functionnal implementation without worries for optimisation. We have benchmarked this implementation. This will serve as a baseline for out future implementations and allow us to identify where optimisations bring most values.

## Benchmarks (V1)

| Orders | Insertions | Insertion / order | Matching  | Total     |
|-------:|------------------:|-------------------:|----------:|----------:|
| 10     | 96 µs               | 9.6 µs             | 2 µs      | 98 µs      |
| 1 000  | 487 µs             | 0.487 µs           | 5855 µs    | 6342 µs    |
| 5000 | 2426 µs | 0.4852 µs | 113976 µs | 116402 µs

As expected, this implementation performs poorly with the **O(n^2) Matching** being the main bottleneck.

## Limitations

- **Matching O(n^2)** : currently we have a double loop on all buyers and sellers.
- **Vector never cleaned**: I used lazy deletion with a flag `cancelled` instead of really modifying the vector. This should change
- **Order Map** : The map is currently useless with the lazy deletion, there is no need to get orders by id in the current implementation.
- **No price-time priority** : The same price orders are not sorted by "arrival time".
- **Precision loss** : `priceMatch()` method silently removes the decimal precision by casting to `int`.
- **No correction/validation**: right now the validation is only visual if you have less than 20 orders (verbose mode on). There should be a verification as part of the test to ensure we are doing the correct matching logic.

These are all amelioration vectors for the next implementation.