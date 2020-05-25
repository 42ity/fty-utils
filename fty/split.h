#pragma once
#include "convert.h"
#include "flags.h"
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fty {

// ===========================================================================================================

/// Split behavior options
enum class SplitOption
{
    SkipEmpty = 1 << 0, //! If a field is empty, don't include it in the result.
    KeepEmpty = 1 << 1, //! If a field is empty, keep it in the result.
    Trim      = 1 << 2, //! Trim a field before add it to the result.
    NoTrim    = 1 << 3, //! Do not trim a field before add it to the result.
};

ENABLE_FLAGS(SplitOption)

// ===========================================================================================================

/// Removes a whitespaces from the start and the end.
/// @param str string to trim
void trim(std::string& str);

/// Returns a string that has whitespace removed from the start and the end.
/// @param str string to trim
/// @return trimmed string
std::string trimmed(const std::string& str);

/// Splits the string into substrings wherever @ref delim occurs. If @ref delim does not match anywhere in the
/// string, split() returns a single-element list containing this string.
/// @param str string to split
/// @param delim delimeter to split
/// @param opt split options
std::vector<std::string> split(const std::string& str, const std::string& delim,
    SplitOption opt = SplitOption::SkipEmpty | SplitOption::Trim);

/// Splits the string into substrings wherever the regular expression @ref delim matches, and returns the list
/// of those strings. If @ref delim does not match anywhere in the string, split() returns a single-element
/// list containing this string.
/// @param str string to split
/// @param delim regular expression to split
/// @param opt split options
std::vector<std::string> split(const std::string& str, const std::regex& delim,
    SplitOption opt = SplitOption::SkipEmpty | SplitOption::Trim);

/// Splits the string into typed tuple wherever the @ref delim occurs matches. In case if split will produce
/// less values then tuple size then will contain default values. If split will produce more values, unused
/// parts will be ingnored
/// @param str string to split
/// @param delim regular expression to split
/// @param opt split options
template <typename... T>
std::tuple<T...> split(const std::string& str, const std::string& delim,
    SplitOption opt = SplitOption::KeepEmpty | SplitOption::Trim);

/// Splits the string into typed tuple wherever the regular expression @ref delim occurs matches. In case if
/// split will produce less values then tuple size then will contain default values. If split will produce
/// more values, unused parts will be ingnored
/// @param str string to split
/// @param delim regular expression to split
/// @param opt split options
template <typename... T>
std::tuple<T...> split(const std::string& str, const std::regex& delim,
    SplitOption opt = SplitOption::KeepEmpty | SplitOption::Trim);

// ===========================================================================================================


namespace detail {

    template <typename... T, std::size_t... Idx>
    std::tuple<T...> vectorToTuple(const std::vector<std::string>& val, std::index_sequence<Idx...>)
    {
        return {convert<T>(Idx < val.size() ? val[Idx] : "")...};
    }

    inline void addString(std::vector<std::string>& ret, SplitOption opt, const std::string& val)
    {
        if (isSet(opt, SplitOption::SkipEmpty) && val.empty()) {
            return;
        }

        if (isSet(opt, SplitOption::Trim)) {
            ret.push_back(trimmed(val));
        } else {
            ret.push_back(val);
        }
    }

} // namespace detail

inline void trim(std::string& str)
{
    static std::string toTrim = " \t\n\r";
    str.erase(str.find_last_not_of(toTrim) + 1);
    str.erase(0, str.find_first_not_of(toTrim));
}

inline std::string trimmed(const std::string& str)
{
    std::string ret = str;
    trim(ret);
    return ret;
}

inline std::vector<std::string> split(const std::string& str, const std::string& delim, SplitOption opt)
{
    size_t                   pos   = 0;
    size_t                   begin = 0;
    std::vector<std::string> ret;

    while ((pos = str.find(delim, begin)) != std::string::npos) {
        detail::addString(ret, opt, str.substr(begin, pos - begin));
        begin = pos + 1;
    }

    if (begin < str.size()) {
        detail::addString(ret, opt, str.substr(begin, str.size() - begin));
    }

    return ret;
}

inline std::vector<std::string> split(const std::string& str, const std::regex& delim, SplitOption opt)
{
    std::vector<std::string>   ret;
    if (!delim.mark_count()) {
        std::sregex_token_iterator iter(str.begin(), str.end(), delim, -1);
        std::sregex_token_iterator end;
        for (; iter != end; ++iter) {
            detail::addString(ret, opt, *iter);
        }
    } else {
        std::vector<int> submatches;
        for(size_t i = 1; i <= delim.mark_count(); ++i) {
            submatches.push_back(int(i));
        }
        std::sregex_token_iterator iter(str.begin(), str.end(), delim, submatches);
        std::sregex_token_iterator end;
        for (; iter != end; ++iter) {
            detail::addString(ret, opt, *iter);
        }
    }
    return ret;
}

template <typename... T>
std::tuple<T...> split(const std::string& str, const std::string& delim, SplitOption opt)
{
    return detail::vectorToTuple<T...>(split(str, delim, opt), std::make_index_sequence<sizeof...(T)>());
}

template <typename... T>
std::tuple<T...> split(const std::string& str, const std::regex& delim, SplitOption opt)
{
    return detail::vectorToTuple<T...>(split(str, delim, opt), std::make_index_sequence<sizeof...(T)>());
}

template <typename Cnt>
std::string implode(const Cnt& cnt, const std::string& delim)
{
    bool              first = true;
    std::stringstream ss;
    for (const auto& it : cnt) {
        if (!first) {
            ss << delim;
        }
        first = false;
        ss << it;
    }
    return ss.str();
}

} // namespace fty
