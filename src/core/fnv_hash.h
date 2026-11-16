#pragma once

#include <string_view>
#include <cstdint>

namespace copyheap {

inline uint64_t fnv1a_64(std::string_view s) noexcept {
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;

    uint64_t hash = FNV_OFFSET_BASIS;
    for (unsigned char c : s) {
        hash ^= static_cast<uint64_t>(c);
        hash *= FNV_PRIME;
    }
    return hash;
}

} // namespace copyheap
