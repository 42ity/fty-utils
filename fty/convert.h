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
#include "traits.h"
#include <sstream>
#include <string>
#include <type_traits>

namespace fty {

template <typename T, typename VT>
T convert(const VT& value)
{
    if constexpr (std::is_constructible_v<std::string, VT>) {
        std::string string(value);
        if (string.empty()) {
            return T{};
        }
        if constexpr (std::is_same_v<T, std::string>) {
            return string;
        } else if constexpr (std::is_same_v<T, bool>) {
            return string == "1" || string == "true";
        } else if constexpr (std::is_same_v<T, float>) {
            return std::stof(string);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::stod(string);
        } else if constexpr (std::is_same_v<T, int8_t>) {
            return int8_t(std::stoi(string));
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            return uint8_t(std::stoul(string));
        } else if constexpr (std::is_same_v<T, int16_t>) {
            return int16_t(std::stoi(string));
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            return uint16_t(std::stoul(string));
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return int32_t(std::stoi(string));
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return uint32_t(std::stoul(string));
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::stoll(string);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return std::stoull(string);
        } else if constexpr (std::is_same_v<T, unsigned char>) {
            return static_cast<unsigned char>(string[0]);
        } else {
            static_assert(fty::always_false<T>, "Unsupported type");
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if constexpr (std::is_integral_v<VT> && !std::is_same_v<bool, VT>) {
            return std::to_string(value);
        } else if constexpr (std::is_floating_point_v<VT>) {
            // In case of floats std::to_string gives not pretty results. So, just use stringstream here.
            std::stringstream ss;
            ss << value;
            return ss.str();
        } else if constexpr (std::is_same_v<bool, VT>) {
            return value ? "true" : "false";
        } else {
            static_assert(fty::always_false<T>, "Unsupported type");
        }
    } else {
        return static_cast<T>(value);
    }
}

// template<typename To, typename From>
// using isConvertable = std::is_same<decltype(convert<To, From>(std::declval<const From&>())), To>;

// template<typename To, typename From, typename = void>
// struct isConvertable : std::false_type
//{};

// template<typename To, typename From>
// struct isConvertable<To, From, std::is_same_v<convert_t<From>, To> : std::true_type
//{
//};

} // namespace fty
