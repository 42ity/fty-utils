#pragma once
#include "convert.h"
#include <cassert>
#include <string>

namespace fty {

struct Unexpected;

// ===========================================================================================================

/// Utilite class simple implemented expected object (Value or error)
/// See http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0323r9.html
template <typename T>
class Expected
{
public:
    constexpr Expected() = delete;
    constexpr Expected(const T& value) noexcept;
    constexpr Expected(T&& value) noexcept;
    constexpr Expected(Unexpected&& unex) noexcept;
    constexpr Expected(const Unexpected& unex) noexcept;
    ~Expected();

    Expected(const Expected&) = delete;
    Expected& operator=(const Expected&) = delete;

    constexpr const T&           value() const& noexcept;
    constexpr T&                 value() & noexcept;
    constexpr const T&&          value() const&& noexcept;
    constexpr T&&                value() && noexcept;
    constexpr const std::string& error() const& noexcept;

    constexpr bool isValid() const noexcept;
    constexpr      operator bool() const noexcept;

    constexpr const T&  operator*() const& noexcept;
    constexpr T&        operator*() & noexcept;
    constexpr const T&& operator*() const&& noexcept;
    constexpr T&&       operator*() && noexcept;
    constexpr const T*  operator->() const noexcept;
    constexpr T*        operator->() noexcept;

private:
    union {
        T           m_value;
        std::string m_error;
    };
    bool m_isError = false;
};

// ===========================================================================================================

struct Unexpected
{
    std::string message;

    template <typename T>
    Unexpected& operator<<(const T& val);
};

// ===========================================================================================================

inline Unexpected unexpected(const std::string& error = {})
{
    return {error};
}

// ===========================================================================================================

template <typename T>
constexpr Expected<T>::Expected(const T& value) noexcept
    : m_value(value)
{
}

template <typename T>
constexpr Expected<T>::Expected(T&& value) noexcept
    : m_value(std::move(value))
{
}

template <typename T>
constexpr Expected<T>::Expected(Unexpected&& unex) noexcept
    : m_error(std::move(unex.message))
    , m_isError(true)
{
}

template <typename T>
constexpr Expected<T>::Expected(const Unexpected& unex) noexcept
    : m_error(unex.message)
    , m_isError(true)
{
}

template <typename T>
Expected<T>::~Expected()
{
    if (m_isError) {
        m_error.~basic_string();
    } else {
        m_value.~T();
    }
}

template <typename T>
constexpr const T& Expected<T>::value() const& noexcept
{
    assert(!m_isError);
    return m_value;
}

template <typename T>
constexpr T& Expected<T>::value() & noexcept
{
    assert(!m_isError);
    return m_value;
}

template <typename T>
constexpr const T&& Expected<T>::value() const&& noexcept
{
    assert(!m_isError);
    return std::move(m_value);
}

template <typename T>
constexpr T&& Expected<T>::value() && noexcept
{
    assert(!m_isError);
    return std::move(m_value);
}

template <typename T>
constexpr const std::string& Expected<T>::error() const& noexcept
{
    assert(m_isError);
    return m_error;
}

template <typename T>
constexpr bool Expected<T>::isValid() const noexcept
{
    return m_isError;
}

template <typename T>
constexpr Expected<T>::operator bool() const noexcept
{
    return !m_isError;
}

template <typename T>
constexpr const T& Expected<T>::operator*() const& noexcept
{
    assert(!m_isError);
    return m_value;
}

template <typename T>
constexpr T& Expected<T>::operator*() & noexcept
{
    assert(!m_isError);
    return m_value;
}

template <typename T>
constexpr const T&& Expected<T>::operator*() const&& noexcept
{
    assert(!m_isError);
    return std::move(m_value);
}

template <typename T>
constexpr T&& Expected<T>::operator*() && noexcept
{
    assert(!m_isError);
    return std::move(m_value);
}


template <typename T>
constexpr const T* Expected<T>::operator->() const noexcept
{
    assert(!m_isError);
    return &m_value;
}

template <typename T>
constexpr T* Expected<T>::operator->() noexcept
{
    assert(!m_isError);
    return &m_value;
}

template <typename T>
Unexpected& Unexpected::operator<<(const T& val)
{
    message += fty::convert<std::string>(val);
    return *this;
}

// ===========================================================================================================

} // namespace fty
