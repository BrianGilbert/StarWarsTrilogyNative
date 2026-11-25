// FNV-1a, 64 bits: the one hash of the project, for recognising the 2D
// layers' backdrops (sky_pack.cpp, tilegen.cpp). It mixes in whole values
// (BGRA pixels, tile entries...), not their bytes.
#pragma once
#include <cstddef>
#include <cstdint>

constexpr uint64_t FNV1A_BASIS = 0xcbf29ce484222325ull, FNV1A_PRIME = 0x100000001b3ull;

// One step: a value (a byte, a word or anything else) mixed into h
inline uint64_t fnv1a_mix(uint64_t h, uint64_t v) { return (h ^ v) * FNV1A_PRIME; }

inline uint64_t fnv1a_words(const uint32_t* p, size_t n, uint64_t h = FNV1A_BASIS) {
    for (size_t i = 0; i < n; i++) h = fnv1a_mix(h, p[i]);
    return h;
}
