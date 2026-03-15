# Database Systems: Data Skipping Programming Assignment

Modern analytical databases use **data skipping** to avoid scanning irrelevant data blocks.
Your task is to design a data skipping index for a simplified scenario.
For more context, see the [TUMuchData Blog — Data Skipping](https://www.tumuchdata.club/post/data-skipping/).

## Task Description

Design a custom data skipping index structure that helps decide whether a data block can be skipped for a given query.
The goal is to minimize the total workload cost consisting of **storage cost** and **access cost**.

Each query is of the form:

    SELECT COUNT(*) FROM table WHERE column = x;

The table consists of a single column of unsigned 32-bit integers.

For each block of data, the index must report how many times the value `x` appears.
The index may return the exact count or indicate "unknown" — in which case the query engine computes the result from the base data.
A single wrong response invalidates the entire run.

### Interface

Your implementation goes in `code/User.hpp`. You need to implement two functions:

```cpp
std::vector<std::byte> build_idx(std::span<const uint32_t> data, Parameters config)
```

`build_idx` receives the contents of a data block and a workload configuration. The configuration includes $F_A$ (access cost weight) and $F_S$ (storage cost weight). Build a custom index and return its content as raw bytes.

Hint: use `reinterpret_cast`.

```cpp
std::optional<size_t> query_idx(uint32_t predicate, const std::vector<std::byte>& index)
```

`query_idx` receives the index previously built for a block and decides whether scanning that block can be skipped. If the index can answer the query, return the count of `predicate` in the block; otherwise return `std::nullopt`.

### Scoring Formula

Total Workload Cost = Access Cost + Storage Cost

```math
Score = (F_A \times \text{\# skipped blocks}) - (F_S \times \text{size of index in KiB})
```

Normalized score (100 points per test case):

```math
Normalized\ Score = 100 \times \frac{Score}{F_A \times \text{\#blocks} \times \text{\#queries}}
```

Correctly skipping blocks earns points; index storage costs deduct points.
$F_A$ and $F_S$ indicate the relative weight of each cost type. Your solution should be robust to varying parameters.

### Test Cases

Your solution is evaluated on 2 test cases with different cost parameters:

| Test Case | $F_A$ | $F_S$ | Description |
|-----------|-------|-------|-------------|
| **frequent** | 3 | 1 | Storage is cheap. Larger, more accurate indices pay off. |
| **normal** | 1 | 5 | Storage is moderately expensive. Be selective about what to store. |

Both test cases use 512 data blocks × 131,072 unsigned 32-bit integers each, with 512 queries.

### Constraints

- $1 \leq F_A, F_S \leq 100$
- Each data block contains 131,072 unsigned 32-bit integers

## Getting Started

**Step 1.** Generate test data (requires Python + NumPy):

```bash
cd code
make generate_data
```

**Step 2.** Implement your solution in `code/User.hpp` — this is the only file you need to modify.

**Step 3.** Build and evaluate:

```bash
cd code
make run_all
```

Your total score is the sum of normalized scores across both test cases (max 100 per case, **200 total**).

You can also run individual test cases: `make run_frequent`, `make run_normal`.

## Grading Rubric

Complete the assignment by following the steps below. Each step builds on the previous one.

**Score requirements** (total = `frequent` + `normal`, out of 200):

| Step | Points | Minimum Score Required |
|------|--------|----------------------|
| Step 1 — Min-Max Index | 30 pts | ≥ 2 |
| Step 2 — Bloom Filter | 30 pts | ≥ 80 |
| Step 3 — Exact Count for Frequent Values | 40 pts | ≥ 105 |
| Bonus: Step 4 — Compression | +10 pts | ≥ 120 |

### Step 1 — Min-Max Index (30 pts)

The simplest data skipping index. This is the baseline used by real-world systems such as Snowflake and Databricks.

**What to do:**
- In `build_idx`, store the minimum and maximum value of the block (just 8 bytes).
- In `query_idx`, if `predicate < min || predicate > max`, the value cannot exist in this block — return `0` to skip it. Otherwise, return `std::nullopt`.

**What you learn:** Even this minimal structure can skip a significant fraction of blocks at negligible storage cost.

### Step 2 — Bloom Filter (30 pts)

A Bloom filter is a compact probabilistic data structure that tests set membership. It has **no false negatives** (if it says "not present", the value is definitely absent) but may have **false positives**.

**What to do:**
- In `build_idx`, construct a Bloom filter from all values in the block, and store it alongside the min-max values.
- In `query_idx`, first apply the min-max check. Then, if the predicate is in range, query the Bloom filter. If the filter says "not present", return `0`. Otherwise, return `std::nullopt`.
- Consider how to size the filter: a larger filter has fewer false positives (more skipping) but higher storage cost. The ratio $F_A / F_S$ should guide this trade-off.

**What you learn:** Probabilistic data structures let you trade a small amount of storage for substantially better skip rates.

### Step 3 — Exact Count for Frequent Values (40 pts)

When some values appear very frequently in a block, storing their exact counts lets you answer queries without scanning.

**What to do:**
- In `build_idx`, identify the top-K most frequent values in the block. Store each value and its count alongside your existing index structures.
- In `query_idx`, if the predicate matches a stored value, return the exact count. Otherwise, fall back to the Bloom filter / min-max check.
- Choose K based on the storage budget: when $F_A / F_S$ is large (storage is cheap), you can afford to store more entries.

**What you learn:** Combining multiple index strategies yields much better results than any single approach. Data-dependent decisions (choosing K) are important for real-world index design.

### Bonus: Step 4 — Compression (up to +10 pts)

When storage is cheap, the ideal index stores exact counts for **all** values in the block — but a naive implementation wastes space.

**What to do:**
- Implement at least one compression technique to reduce index size. Options include:
  - **Delta encoding** — store sorted values as successive differences
  - **Varint encoding** — encode small integers using fewer bytes
  - **Bitpacking** — pack values using only as many bits as needed
  - **Run-length encoding (RLE)** — exploit repeated values
- When $F_A / F_S$ is high enough, switch from TopK to a full compressed count map.

**What you learn:** Encoding schemes are critical in database systems — the same logical data can vary enormously in physical size.

## Project Structure

```
.
├── README.md              # This file
├── code/
│   ├── User.hpp           # ★ The file you need to modify
│   ├── Makefile           # Build & run targets
│   ├── main.cpp           # Evaluation harness
│   ├── Parameters.hpp     # Config (f_a, f_s)
│   └── FileUtils.hpp      # File I/O
└── data/
    ├── eval.json          # Test case configs
    ├── generate.py        # Data generator
    └── generate_all.py    # Generate all test cases
```

## Acknowledgments

This assignment is adapted from the [TUMuchData Coding Challenge 2025](https://github.com/tumuchdata/coding-challenge-2025-cpp). Thanks to [TUMuchData](http://tumuchdata.club) for the excellent problem framework.
