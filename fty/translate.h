/*  ====================================================================================================================
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
    ====================================================================================================================
*/

#pragma once
#include <fmt/format.h>
#include <functional>
#include <string>

// =====================================================================================================================

namespace fty {

/// Translation message storage and formater.
/// Typical usage is:
///     Translate("parrot: {} {}").format("norwegian", "blue").toString()
class Translate
{
public:
    using FormatFunc = std::function<std::string(const std::string&)>;

public:
    Translate(const std::string& msg)
        : m_msg(msg)
        , m_format([&](const std::string& s) {
            return translate(s);
        })
    {
    }

    Translate(const Translate&) = default;
    Translate(Translate&&)      = default;
    Translate& operator=(const Translate&) = default;
    Translate& operator=(Translate&&) = default;

    operator std::string() const
    {
        return toString();
    }

    std::string toString() const
    {
        return m_format(m_msg);
    }

    template <typename... Args>
    Translate& format(const Args&... args)
    {
        m_format = [&](const std::string& msg) {
            return fmt::format(translate(msg), args...);
        };
        return *this;
    }

public:
    const std::string& message() const
    {
        return m_msg;
    }

    const FormatFunc& formatFunc() const
    {
        return m_format;
    }

private:
    std::string translate(const std::string& str)
    {
        // TODO: Add string translation
        return str;
    }

private:
    std::string m_msg;
    FormatFunc  m_format;
};

/// Creates translate object
template <size_t N>
inline Translate tr(const char (&str)[N])
{
    return Translate(str);
}

} // namespace fty

// =====================================================================================================================

/// Creates translate string literal
inline fty::Translate operator"" _tr(const char* str, size_t size)
{
    return fty::Translate(std::string(str, size));
}

/// Inserts translate into string
inline std::ostream& operator<<(std::ostream& s, const fty::Translate& tr)
{
    s << tr.toString();
    return s;
}

/// Allow string concatination
inline std::string operator+(const std::string& a, const fty::Translate& b)
{
    return a + b.toString();
}

inline std::string operator+(const fty::Translate& a, const std::string& b)
{
    return a.toString() + b;
}

// =====================================================================================================================

/// Helper to format translate to fmt
template <>
struct fmt::formatter<fty::Translate>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const fty::Translate& trans, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "{}", trans.toString());
    }
};
