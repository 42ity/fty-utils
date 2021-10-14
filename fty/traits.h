/*  ========================================================================
    Copyright (C) 2021 Eaton
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
#include <type_traits>

namespace fty {

template <class...>
constexpr std::false_type always_false{};

template <typename... T>
struct is_cont_helper
{
};

template <typename T, typename = void>
struct is_list : std::false_type
{
};

template <typename T, typename = void>
struct is_map : std::false_type
{
};

// clang-format off
template <typename T>
struct is_list<T, std::conditional_t<false,
    is_cont_helper<
        typename T::value_type,
        typename T::size_type,
        decltype(std::declval<T>().begin()),
        decltype(std::declval<T>().end())
    >,
    void>> : public std::true_type
{
};

template <typename T>
struct is_map<T, std::conditional_t<false,
    is_cont_helper<
        typename T::key_type,
        typename T::value_type,
        typename T::size_type,
        decltype(std::declval<T>().begin()),
        decltype(std::declval<T>().end())
    >,
    void>> : public std::true_type
{
};

}
