#include <optional>
#include <vector>
#include <span>
#include <cstdint>
#include <cassert>
#include <cstring> 

#include "Parameters.hpp"

struct MinMax {
    uint32_t min;
    uint32_t max;
};

std::vector<std::byte> build_idx(std::span<const uint32_t> data, Parameters config){
    MinMax mm{UINT32_MAX, 0};

    for (auto v : data) {
        if (v < mm.min) mm.min = v;
        if (v > mm.max) mm.max = v;
    }

    size_t bloom_bits = (config.f_a > config.f_s) ? 8192 : 2048;
    size_t bloom_bytes = bloom_bits / 8;

    std::vector<std::byte> result(sizeof(MinMax) + bloom_bytes);
    std::memcpy(result.data(), &mm, sizeof(MinMax));

    uint8_t* bloom = reinterpret_cast<uint8_t*>(result.data() + sizeof(MinMax));

    for (auto v : data) {
        uint32_t h1 = hash1(v) % bloom_bits;
        uint32_t h2 = hash2(v) % bloom_bits;

        bloom[h1 / 8] |= (1 << (h1 % 8));
        bloom[h2 / 8] |= (1 << (h2 % 8));
    }

    return result;
    
}

std::optional<size_t> query_idx(uint32_t predicate, const std::vector<std::byte>& index){
    const MinMax* mm = reinterpret_cast<const MinMax*>(index.data());

    if (predicate < mm->min || predicate > mm->max) {
        return 0;
    }

    return std::nullopt;
}