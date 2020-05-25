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
