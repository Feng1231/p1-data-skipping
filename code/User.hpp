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
// Hash functions suggested by chatGPT
uint32_t hash1(uint32_t x) { return x * 2654435761u; }
uint32_t hash2(uint32_t x) { return (x ^ 0xdeadbeef) * 1597334677u; }

std::vector<std::byte> build_idx(std::span<const uint32_t> data, Parameters config) {
    MinMax mm{UINT32_MAX, 0};
    for (auto v : data) {
        if (v < mm.min) mm.min = v;
        if (v > mm.max) mm.max = v;
    }

    
    std::unordered_map<uint32_t, uint32_t> freq;
    for (auto v : data) freq[v]++;

    size_t K = (config.f_a > config.f_s) ? 32 : 8;

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

    size_t bloom_bits = (config.f_a > config.f_s) ? 8192 : 2048;
    size_t bloom_bytes = bloom_bits / 8;

    size_t total_size =
        sizeof(MinMax) +
        sizeof(uint32_t) +                 
        actual_K * sizeof(uint32_t) * 2 + bloom_bytes;

    std::vector<std::byte> result(total_size);

    std::byte* ptr = result.data();

    std::memcpy(ptr, &mm, sizeof(MinMax));
    ptr += sizeof(MinMax);

    std::memcpy(ptr, &actual_K, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    for (auto& [value, count] : vec) {
        std::memcpy(ptr, &value, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        std::memcpy(ptr, &count, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    uint8_t* bloom = reinterpret_cast<uint8_t*>(ptr);
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

    uint32_t K;
    std::memcpy(&K, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    for (uint32_t i = 0; i < K; i++) {
        uint32_t value, count;

        std::memcpy(&value, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        std::memcpy(&count, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        if (value == predicate) {
            return count;
        }
    }

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