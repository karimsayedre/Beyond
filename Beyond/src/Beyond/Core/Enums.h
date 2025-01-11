#pragma once

#include <type_traits>
#include <compare>

#define BEY_ENABLE_ENUM_OPERATORS(EnumType) \
inline EnumType operator+(EnumType lhs, EnumType rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) + static_cast<std::underlying_type<EnumType>::type>(rhs)); } \
inline EnumType operator-(EnumType lhs, EnumType rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) - static_cast<std::underlying_type<EnumType>::type>(rhs)); } \
inline EnumType operator*(EnumType lhs, EnumType rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) * static_cast<std::underlying_type<EnumType>::type>(rhs)); } \
inline EnumType operator/(EnumType lhs, EnumType rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) / static_cast<std::underlying_type<EnumType>::type>(rhs)); } \
inline EnumType& operator+=(EnumType& lhs, EnumType rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) + static_cast<std::underlying_type<EnumType>::type>(rhs)); return lhs; } \
inline EnumType& operator-=(EnumType& lhs, EnumType rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) - static_cast<std::underlying_type<EnumType>::type>(rhs)); return lhs; } \
inline EnumType& operator*=(EnumType& lhs, EnumType rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) * static_cast<std::underlying_type<EnumType>::type>(rhs)); return lhs; } \
inline EnumType& operator/=(EnumType& lhs, EnumType rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) / static_cast<std::underlying_type<EnumType>::type>(rhs)); return lhs; } \
inline std::strong_ordering operator<=>(EnumType lhs, EnumType rhs) { return static_cast<std::underlying_type<EnumType>::type>(lhs) <=> static_cast<std::underlying_type<EnumType>::type>(rhs); } \
inline bool operator==(EnumType lhs, EnumType rhs) { return static_cast<std::underlying_type<EnumType>::type>(lhs) == static_cast<std::underlying_type<EnumType>::type>(rhs); } \
inline EnumType operator|(EnumType lhs, EnumType rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) | static_cast<std::underlying_type<EnumType>::type>(rhs)); } \
inline EnumType operator&(EnumType lhs, EnumType rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) & static_cast<std::underlying_type<EnumType>::type>(rhs)); } \
inline EnumType operator^(EnumType lhs, EnumType rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) ^ static_cast<std::underlying_type<EnumType>::type>(rhs)); } \
inline EnumType operator~(EnumType lhs) { return static_cast<EnumType>(~static_cast<std::underlying_type<EnumType>::type>(lhs)); } \
inline EnumType& operator|=(EnumType& lhs, EnumType rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) | static_cast<std::underlying_type<EnumType>::type>(rhs)); return lhs; } \
inline EnumType& operator&=(EnumType& lhs, EnumType rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) & static_cast<std::underlying_type<EnumType>::type>(rhs)); return lhs; } \
inline EnumType& operator^=(EnumType& lhs, EnumType rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) ^ static_cast<std::underlying_type<EnumType>::type>(rhs)); return lhs; } \
inline EnumType operator+(EnumType lhs, int rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) + rhs); } \
inline EnumType operator-(EnumType lhs, int rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) - rhs); } \
inline EnumType operator*(EnumType lhs, int rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) * rhs); } \
inline EnumType operator/(EnumType lhs, int rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) / rhs); } \
inline EnumType& operator+=(EnumType& lhs, int rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) + rhs); return lhs; } \
inline EnumType& operator-=(EnumType& lhs, int rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) - rhs); return lhs; } \
inline EnumType& operator*=(EnumType& lhs, int rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) * rhs); return lhs; } \
inline EnumType& operator/=(EnumType& lhs, int rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) / rhs); return lhs; } \
inline std::strong_ordering operator<=>(EnumType lhs, int rhs) { return static_cast<std::underlying_type<EnumType>::type>(lhs) <=> rhs; } \
inline bool operator==(EnumType lhs, int rhs) { return static_cast<std::underlying_type<EnumType>::type>(lhs) == rhs; } \
inline EnumType operator|(EnumType lhs, int rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) | rhs); } \
inline EnumType operator&(EnumType lhs, int rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) & rhs); } \
inline EnumType operator^(EnumType lhs, int rhs) { return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) ^ rhs); } \
inline EnumType& operator|=(EnumType& lhs, int rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) | rhs); return lhs; } \
inline EnumType& operator&=(EnumType& lhs, int rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) & rhs); return lhs; } \
inline EnumType& operator^=(EnumType& lhs, int rhs) { lhs = static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(lhs) ^ rhs); return lhs; } \
inline std::strong_ordering operator<=>(int lhs, EnumType rhs) { return lhs <=> static_cast<std::underlying_type<EnumType>::type>(rhs); } \
inline bool operator==(int lhs, EnumType rhs) { return lhs == static_cast<std::underlying_type<EnumType>::type>(rhs); }

