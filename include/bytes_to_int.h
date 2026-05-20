#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

enum class ByteOrder {
    LittleEndian,
    BigEndian
};

template <typename T, ByteOrder Order>
constexpr T bytes_to_int(const uint8_t* bytes) noexcept {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    T value = 0;

    if constexpr (Order == ByteOrder::LittleEndian) {
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            value |= static_cast<T>(bytes[i]) << (8 * i);
        }
    } else {
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            value |= static_cast<T>(bytes[i]) << (8 * (sizeof(T) - 1 - i));
        }
    }

    return value;
}