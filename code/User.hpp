#include <optional>
#include <vector>
#include <span>
#include <cstdint>
#include <cassert>

#include "Parameters.hpp"

struct MinMax {
    uint32_t min;
    uint32_t max;
};

std::vector<std::byte> build_idx(std::span<const uint32_t> data, Parameters config){
    MinMax mm;
    mm.min = UINT32_MAX;
    mm.max = 0;

    for (uint32_t v : data) {
        if (v < mm.min) mm.min = v;
        if (v > mm.max) mm.max = v;
    }

    std::vector<std::byte> idx(sizeof(MinMax));
    std::memcpy(idx.data(), &mm, sizeof(MinMax));
    return idx;
}

std::optional<size_t> query_idx(uint32_t predicate, const std::vector<std::byte>& index){
    const MinMax* mm = reinterpret_cast<const MinMax*>(index.data());

    if (predicate < mm->min || predicate > mm->max) {
        return 0;
    }

    return std::nullopt;
}