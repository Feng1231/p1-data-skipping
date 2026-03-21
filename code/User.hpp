#include <optional>
#include <vector>
#include <span>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <algorithm>

#include "Parameters.hpp"


struct MinMax {
    uint32_t min;
    uint32_t max;
};

static inline uint32_t hash1(uint32_t x) {
    return x * 2654435761u;
}

static inline uint32_t hash2(uint32_t x) {
    return (x ^ 0xdeadbeef) * 1597334677u;
}

// Variable-length encoding for integers (similar to LEB128)

void write_varint(std::vector<std::byte>& out, uint32_t x) {
    while (x >= 128) {
        out.push_back(std::byte((x & 0x7F) | 0x80));
        x >>= 7;
    }
    out.push_back(std::byte(x));
}

uint32_t read_varint(const std::byte*& ptr) {
    uint32_t result = 0;
    int shift = 0;

    while (true) {
        uint8_t byte = static_cast<uint8_t>(*ptr++);
        result |= (byte & 0x7F) << shift;

        if (!(byte & 0x80)) break;
        shift += 7;
    }
    return result;
}


std::vector<std::byte> build_idx(std::span<const uint32_t> data, Parameters config) {
    MinMax mm{UINT32_MAX, 0};
    for (auto v : data) {
        if (v < mm.min) mm.min = v;
        if (v > mm.max) mm.max = v;
    }

    std::unordered_map<uint32_t, uint32_t> freq;
    for (auto v : data) freq[v]++;

    double ratio = (double)config.f_a / config.f_s;
    bool use_full_map = (ratio > 2.5);

    std::vector<std::byte> result;

    // write MinMax
    result.resize(sizeof(MinMax));
    std::memcpy(result.data(), &mm, sizeof(MinMax));

    // write mode
    uint8_t mode = use_full_map ? 1 : 0;
    result.push_back(std::byte(mode));

    // Mode 1: Full Compressed Map
    if (use_full_map) {
        std::vector<std::pair<uint32_t,uint32_t>> vec(freq.begin(), freq.end());

        std::sort(vec.begin(), vec.end(),
            [](auto& a, auto& b) {
                return a.first < b.first;
            });

        uint32_t prev = 0;

        for (size_t i = 0; i < vec.size(); i++) {
            uint32_t value = vec[i].first;
            uint32_t count = vec[i].second;

            uint32_t delta = (i == 0) ? value : (value - prev);

            write_varint(result, delta);
            write_varint(result, count);

            prev = value;
        }

        return result;
    }

    // Mode 0: TopK + Bloom

    // Top K
    size_t K = (ratio > 1.5) ? 32 : 8;

    std::vector<std::pair<uint32_t, uint32_t>> vec(freq.begin(), freq.end());

    if (vec.size() > K) {
        std::partial_sort(
            vec.begin(),
            vec.begin() + K,
            vec.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second;
            }
        );
        vec.resize(K);
    }

    uint32_t actual_K = static_cast<uint32_t>(vec.size());

    // Write K
    size_t old_size = result.size();
    result.resize(old_size + sizeof(uint32_t));
    std::memcpy(result.data() + old_size, &actual_K, sizeof(uint32_t));

    // Write Top K
    for (auto& [value, count] : vec) {
        size_t pos = result.size();
        result.resize(pos + 8);

        std::memcpy(result.data() + pos, &value, 4);
        std::memcpy(result.data() + pos + 4, &count, 4);
    }

    // Bloom Filter
    size_t bloom_bits = (ratio > 1.5) ? 8192 : 2048;
    size_t bloom_bytes = bloom_bits / 8;

    size_t bloom_start = result.size();
    result.resize(bloom_start + bloom_bytes);

    uint8_t* bloom = reinterpret_cast<uint8_t*>(result.data() + bloom_start);
    std::memset(bloom, 0, bloom_bytes);

    for (auto v : data) {
        uint32_t h1 = hash1(v) % bloom_bits;
        uint32_t h2 = hash2(v) % bloom_bits;

        bloom[h1 / 8] |= (1 << (h1 % 8));
        bloom[h2 / 8] |= (1 << (h2 % 8));
    }

    return result;
}

std::optional<size_t> query_idx(uint32_t predicate, const std::vector<std::byte>& index) {
    const std::byte* ptr = index.data();

    const MinMax* mm = reinterpret_cast<const MinMax*>(ptr);
    ptr += sizeof(MinMax);

    if (predicate < mm->min || predicate > mm->max) {
        return 0;
    }

    uint8_t mode = static_cast<uint8_t>(*ptr++);
    
    // Mode 1: Full Compressed Map
    if (mode == 1) {
        uint32_t value = 0;
        bool first = true;

        while (ptr < index.data() + index.size()) {
            uint32_t delta = read_varint(ptr);
            uint32_t count = read_varint(ptr);

            value = first ? delta : value + delta;
            first = false;

            if (value == predicate) return count;
            if (value > predicate) return 0; // early stop
        }

        return 0;
    }


    // Mode 0: TopK + Bloom
   
    // Read k
    uint32_t K;
    std::memcpy(&K, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    //  Top k
    for (uint32_t i = 0; i < K; i++) {
        uint32_t value, count;

        std::memcpy(&value, ptr, 4);
        ptr += 4;

        std::memcpy(&count, ptr, 4);
        ptr += 4;

        if (value == predicate) return count;
    }

    // Bloom Filter
    size_t bloom_bytes = index.size() - (ptr - index.data());
    size_t bloom_bits = bloom_bytes * 8;

    const uint8_t* bloom = reinterpret_cast<const uint8_t*>(ptr);

    uint32_t h1 = hash1(predicate) % bloom_bits;
    uint32_t h2 = hash2(predicate) % bloom_bits;

    bool present =
        (bloom[h1 / 8] & (1 << (h1 % 8))) &&
        (bloom[h2 / 8] & (1 << (h2 % 8)));

    if (!present) return 0;

    return std::nullopt;
}