#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>

// jsf.h — Jenkins Small Fast PRNG, C++17, header-only.
//
// Satisfies UniformRandomBitGenerator, so it works with:
//   std::uniform_int_distribution<>, std::uniform_real_distribution<>,
//   std::shuffle(), etc.
//
// Two ready-made aliases:
//   Jsf32   — 32-bit variant (good for Teensy / embedded)
//   Jsf64   — 64-bit variant (good for desktop / Python cross-check)
//
// Usage:
//   Jsf32 rng(42);
//   uint32_t v  = rng();
//   float    f  = rng.next_float();
//   int      i  = rng.next_in_range(0, 100);
//
//   std::uniform_real_distribution<float> dist(0.f, 1.f);
//   float x = dist(rng);
//
// Reference: https://burtleburtle.net/bob/rand/smallprng.html

namespace jsf_detail {

// ---------------------------------------------------------------------------
// Traits — rotation constants and magic initialiser differ between widths.
// ---------------------------------------------------------------------------
template <typename T>
struct JsfTraits;

template <>
struct JsfTraits<uint32_t> {
    static constexpr int kRotA = 27;
    static constexpr int kRotB = 17;
    static constexpr uint32_t kSeedA = 0xf1ea5eedu;
};

template <>
struct JsfTraits<uint64_t> {
    static constexpr int kRotA = 7;
    static constexpr int kRotB = 13;
    static constexpr int kRotC = 37;   // 64-bit variant has a third rotation
    static constexpr uint64_t kSeedA = 0xf1ea5eedull;
};

// ---------------------------------------------------------------------------
// Portable rotate (branchless, no UB).
// ---------------------------------------------------------------------------
template <typename T>
[[nodiscard]] constexpr T rotl(T x, int k) noexcept {
    return (x << k) | (x >> (std::numeric_limits<T>::digits - k));
}

} // namespace jsf_detail

// ---------------------------------------------------------------------------
// Jsf<T> — the actual generator.
// ---------------------------------------------------------------------------
template <typename T>
class Jsf {
    static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>,
                  "Jsf only supports uint32_t and uint64_t");

    using Traits = jsf_detail::JsfTraits<T>;

public:
    // UniformRandomBitGenerator requirements.
    using result_type = T;
    [[nodiscard]] static constexpr T min() noexcept { return 0; }
    [[nodiscard]] static constexpr T max() noexcept { return ~T{0}; }

    explicit Jsf(T seed = 0) noexcept { this->seed(seed); }

    void seed(T s) noexcept {
        _a = Traits::kSeedA;
        _b = _c = _d = s;
        for (int i = 0; i < 20; ++i) {
            (void)(*this)();  // warm-up
        }
    }

    // Core output — operator() satisfies UniformRandomBitGenerator.
    [[nodiscard]] T operator()() noexcept {
        if constexpr (std::is_same_v<T, uint32_t>) {
            return _next32();
        } else {
            return _next64();
        }
    }

    // Convenience: float in [0.0, 1.0) using full mantissa bits.
    [[nodiscard]] float next_float() noexcept {
        if constexpr (std::is_same_v<T, uint32_t>) {
            // 23-bit mantissa for float; shift right 9.
            return static_cast<float>((*this)() >> 9) * (1.0f / (1u << 23));
        } else {
            // 53-bit mantissa for double-precision float.
            return static_cast<float>(((*this)() >> 11) * (1.0 / (1ull << 53)));
        }
    }

    // Convenience: double in [0.0, 1.0).
    [[nodiscard]] double next_double() noexcept {
        return ((*this)() >> 11) * (1.0 / (1ull << 53));
    }

    // Convenience: uniform integer in [lo, hi] inclusive.
    // Note: modulo bias is negligible for benchmarking; use std::uniform_int_distribution
    // if you need unbiased sampling.
    [[nodiscard]] T next_in_range(T lo, T hi) noexcept {
        return lo + (*this)() % (hi - lo + T{1});
    }

    // Bulk fill — avoids repeated call overhead in tight loops.
    void fill(T* buf, std::size_t n) noexcept {
        for (std::size_t i = 0; i < n; ++i) buf[i] = (*this)();
    }

    void fill(float* buf, std::size_t n) noexcept {
        for (std::size_t i = 0; i < n; ++i) buf[i] = next_float();
    }

private:
    T _a, _b, _c, _d;

    [[nodiscard]] uint32_t _next32() noexcept {
        uint32_t e = _a - jsf_detail::rotl(_b, Traits::kRotA);
        _a = _b ^ jsf_detail::rotl(_c, Traits::kRotB);
        _b = _c + _d;
        _c = _d + e;
        _d = e + _a;
        return _d;
    }

    [[nodiscard]] uint64_t _next64() noexcept {
        uint64_t e = _a - jsf_detail::rotl(_b, Traits::kRotA);
        _a = _b ^ jsf_detail::rotl(_c, Traits::kRotB);
        _b = _c + jsf_detail::rotl(_d, Traits::kRotC);
        _c = _d + e;
        _d = e + _a;
        return _d;
    }
};

// ---------------------------------------------------------------------------
// Aliases.
// ---------------------------------------------------------------------------
using Jsf32 = Jsf<uint32_t>;
using Jsf64 = Jsf<uint64_t>;
