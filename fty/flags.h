#pragma once

#define ENABLE_FLAGS(EnumType)                                                                               \
    constexpr EnumType operator~(EnumType rhs) noexcept                                                      \
    {                                                                                                        \
        return static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(rhs));                   \
    }                                                                                                        \
    constexpr auto operator|(EnumType lhs, EnumType rhs) noexcept                                            \
    {                                                                                                        \
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) |                    \
                                     static_cast<std::underlying_type_t<EnumType>>(rhs));                    \
    }                                                                                                        \
    constexpr auto operator&(EnumType lhs, EnumType rhs) noexcept                                            \
    {                                                                                                        \
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) &                    \
                                     static_cast<std::underlying_type_t<EnumType>>(rhs));                    \
    }                                                                                                        \
    constexpr auto operator^(EnumType lhs, EnumType rhs) noexcept                                            \
    {                                                                                                        \
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) ^                    \
                                     static_cast<std::underlying_type_t<EnumType>>(rhs));                    \
    }                                                                                                        \
    constexpr auto operator|=(EnumType& lhs, EnumType rhs) noexcept                                          \
    {                                                                                                        \
        return lhs = lhs | rhs;                                                                              \
    }                                                                                                        \
    constexpr auto operator&=(EnumType& lhs, EnumType rhs) noexcept                                          \
    {                                                                                                        \
        return lhs = lhs & rhs;                                                                              \
    }                                                                                                        \
    constexpr auto operator^=(EnumType& lhs, EnumType rhs) noexcept                                          \
    {                                                                                                        \
        return lhs = lhs ^ rhs;                                                                              \
    }

namespace fty {

template <typename T>
bool isSet(T flag, T option)
{
    return std::underlying_type_t<T>(flag & option);
}

}
