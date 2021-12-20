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
#include "convert.h"
#include <cassert>
#include <fmt/format.h>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace fty {

template <typename>
struct Unexpected;

// ===========================================================================================================

/// Utilite class simple implemented expected object (Value or error)
/// See http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0323r9.html
template <typename T, typename ErrorT = std::string>
class Expected
{
public:
    constexpr Expected() = default;
    constexpr Expected(const T& value) noexcept;
    constexpr Expected(T&& value) noexcept;
    constexpr Expected(Expected&& other) noexcept;
    template <typename UnErrorT>
    constexpr Expected(Unexpected<UnErrorT>&& unex) noexcept;
    template <typename UnErrorT>
    constexpr Expected(const Unexpected<UnErrorT>& unex) noexcept;
    ~Expected() = default;

    Expected(const Expected& other) = default;
    Expected& operator=(const Expected& other) = default;

    constexpr const T&      value() const&;
    constexpr T&            value() &;
    constexpr const T&&     value() const&&;
    constexpr T&&           value() &&;
    constexpr const ErrorT& error() const&;

    constexpr bool     isValid() const noexcept;
    explicit constexpr operator bool() const noexcept;

    constexpr const T&  operator*() const&;
    constexpr T&        operator*() &;
    constexpr const T&& operator*() const&&;
    constexpr T&&       operator*() &&;
    constexpr const T*  operator->() const noexcept;
    constexpr T*        operator->() noexcept;

private:
    std::variant<T, ErrorT> m_value;
};

template <typename ErrorT>
class Expected<void, ErrorT>
{
public:
    Expected() noexcept = default ;
    template <typename UnErrorT>
    Expected(Unexpected<UnErrorT>&& unex) noexcept;
    template <typename UnErrorT>
    Expected(const Unexpected<UnErrorT>& unex) noexcept;

    Expected(const Expected& other)
    {
        m_error   = std::move(other.error());
        m_isError = other.m_isError;
    };

    Expected& operator=(const Expected&) = default;

    constexpr bool isValid() const noexcept;
    constexpr      operator bool() const noexcept;

    constexpr const ErrorT& error() const& noexcept;

private:
    std::optional<ErrorT> m_error;
    bool                  m_isError = false;
};

// ===========================================================================================================

template <typename ErrorT = std::string>
struct Unexpected
{
    constexpr Unexpected(const ErrorT& value) noexcept;
    ErrorT message;
};

// ===========================================================================================================

template <typename ErrorT>
constexpr Unexpected<ErrorT>::Unexpected(const ErrorT& value) noexcept
    : message(value)
{
}

template <typename ErrorT, typename = std::enable_if_t<!std::is_convertible_v<ErrorT, std::string>>>
inline Unexpected<ErrorT> unexpected(const ErrorT& error = {})
{
    return Unexpected<ErrorT>(error);
}

template <typename... Args>
inline Unexpected<std::string> unexpected(const std::string& fmt, const Args&... args)
{
    try {
        return {fmt::format(fmt, args...)};
    } catch (const fmt::format_error&) {
        assert("Format error");
        return fmt;
    }
}

template <typename ErrorT, typename = std::enable_if_t<std::is_convertible_v<ErrorT, std::string>>>
inline Unexpected<std::string> unexpected(const ErrorT& value)
{
    return {value};
}

// ===========================================================================================================

template <typename T, typename ErrorT>
constexpr Expected<T, ErrorT>::Expected(const T& value) noexcept
    : m_value(value)
{
}

template <typename T, typename ErrorT>
constexpr Expected<T, ErrorT>::Expected(T&& value) noexcept
    : m_value(std::move(value))
{
}

template <typename T, typename ErrorT>
constexpr Expected<T, ErrorT>::Expected(Expected&& other) noexcept
{
    m_value = std::move(other.m_value);
}

template <typename T, typename ErrorT>
template <typename UnErrorT>
constexpr Expected<T, ErrorT>::Expected(Unexpected<UnErrorT>&& unex) noexcept
    : m_value(std::move(unex.message))
{
}

template <typename T, typename ErrorT>
template <typename UnErrorT>
constexpr Expected<T, ErrorT>::Expected(const Unexpected<UnErrorT>& unex) noexcept
    : m_value(unex.message)
{
}

template <typename T, typename ErrorT>
constexpr const T& Expected<T, ErrorT>::value() const&
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr T& Expected<T, ErrorT>::value() &
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr const T&& Expected<T, ErrorT>::value() const&&
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr T&& Expected<T, ErrorT>::value() &&
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr const ErrorT& Expected<T, ErrorT>::error() const&
{
  assert(std::holds_alternative<ErrorT>(m_value));
  return std::get<ErrorT>(m_value);
}

template <typename T, typename ErrorT>
constexpr bool Expected<T, ErrorT>::isValid() const noexcept
{
  return std::holds_alternative<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr Expected<T, ErrorT>::operator bool() const noexcept
{
  return isValid();
}

template <typename T, typename ErrorT>
constexpr const T& Expected<T, ErrorT>::operator*() const&
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr T& Expected<T, ErrorT>::operator*() &
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr const T&& Expected<T, ErrorT>::operator*() const&&
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr T&& Expected<T, ErrorT>::operator*() &&
{
  return std::get<T>(m_value);
}

template <typename T, typename ErrorT>
constexpr const T* Expected<T, ErrorT>::operator->() const noexcept
{
  return std::get_if<T>(&m_value);
}

template <typename T, typename ErrorT>
constexpr T* Expected<T, ErrorT>::operator->() noexcept
{
  return std::get_if<T>(&m_value);
}

// ===========================================================================================================

template <typename ErrorT>
template <typename UnErrorT>
inline Expected<void, ErrorT>::Expected(Unexpected<UnErrorT>&& unex) noexcept
    : m_error(std::move(unex.message))
    , m_isError(true)
{
}

template <typename ErrorT>
template <typename UnErrorT>
inline Expected<void, ErrorT>::Expected(const Unexpected<UnErrorT>& unex) noexcept
    : m_error(unex.message)
    , m_isError(true)
{
}

template <typename ErrorT>
inline constexpr bool Expected<void, ErrorT>::isValid() const noexcept
{
    return !m_isError;
}

template <typename ErrorT>
inline constexpr Expected<void, ErrorT>::operator bool() const noexcept
{
    return isValid();
}

template <typename ErrorT>
inline constexpr const ErrorT& Expected<void, ErrorT>::error() const& noexcept
{
    assert(m_error != std::nullopt);
    return *m_error;
}

// ===========================================================================================================

} // namespace fty
