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
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return std::stoi(string);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return uint32_t(std::stoul(string));
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::stoll(string);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return std::stoull(string);
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

//template<typename To, typename From>
//using isConvertable = std::is_same<decltype(convert<To, From>(std::declval<const From&>())), To>;

//template<typename To, typename From, typename = void>
//struct isConvertable : std::false_type
//{};

//template<typename To, typename From>
//struct isConvertable<To, From, std::is_same_v<convert_t<From>, To> : std::true_type
//{
//};

} // namespace fty
