/*  ========================================================================
    Copyright (C) 2020 Eaton
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ========================================================================
*/
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
