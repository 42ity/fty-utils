#pragma once

#include "convert.h"
#include "split.h"
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

namespace fty {

class CommandLine
{
public:
    struct OptionDef
    {
        template <typename T>
        OptionDef(const std::string& fmt, T& var, const std::string& descr);

        std::function<void(const std::string&)> setter;
        std::string                             format;
        std::string                             description;
        bool                                    isBool;
        std::string                             def;
    };

    class Option
    {
    public:
        Option(const OptionDef& option);
        Option(const Option&) = delete;
        Option(Option&&)      = default;
        Option& operator=(const Option&) = delete;
        Option& operator=(Option&&) = default;

        void setOneOfMany(const std::vector<std::string> values);

    private:
        bool        isBoolOpt();
        bool        match(const std::string& value);
        void        consume(std::vector<std::string>& args);
        std::string optFormatHelp() const;
        std::string helpDesc() const;

        void setValue(const std::string& str);

    private:
        struct Format
        {
            Format(const std::string& format);

            std::string longFormat;
            std::string shortFormat;
        } m_format;

        std::vector<std::string> m_oneOfMany;
        OptionDef                m_option;
        friend class CommandLine;
    };

public:
    CommandLine(const std::string& description, std::initializer_list<OptionDef> options);

    bool                            parse(int argc, char** argv);
    std::string                     help() const;
    const std::vector<std::string>& positionalArgs() const;
    Option&                         option(const std::string& key);
    const std::string&              error() const;

private:
    std::string                          m_description;
    std::vector<std::unique_ptr<Option>> m_options;
    std::vector<std::string>             m_positionalArgs;
    std::string                          m_error;
};

// ===========================================================================================================

template <typename T>
CommandLine::OptionDef::OptionDef(const std::string& fmt, T& var, const std::string& descr)
    : setter([&](const std::string& val) {
        if constexpr (std::is_same_v<bool, T>) {
            var = true;
        } else {
            var = convert<T>(val);
        }
    })
    , format(fmt)
    , description(descr)
    , isBool(std::is_same_v<T, bool>)
    , def(var != T{} ? convert<std::string>(var) : "")
{
}

// ===========================================================================================================

CommandLine::Option::Option(const OptionDef& option)
    : m_format(option.format)
    , m_option(option)
{
}

bool CommandLine::Option::isBoolOpt()
{
    return m_option.isBool;
}

bool CommandLine::Option::match(const std::string& value)
{
    if (!m_format.longFormat.empty() && value.find(m_format.longFormat) == 0) {
        return true;
    }
    return value == m_format.shortFormat;
}

void CommandLine::Option::consume(std::vector<std::string>& args)
{
    for(auto it = args.begin(); it != args.end(); ){
        if (!match(*it)) {
            ++it;
            continue;
        }

        const std::string& arg = *it;

        if (!m_format.longFormat.empty() && arg.find(m_format.longFormat) == 0) {
            auto pos = arg.find("=");
            if (isBoolOpt()) {
                m_option.setter("");
            } else if (pos == std::string::npos) {
                throw std::runtime_error("Wrong format of option " + arg);
            }
            setValue(arg.substr(pos + 1));
            args.erase(it);
        }
        if (arg == m_format.shortFormat) {
            if (isBoolOpt()) {
                m_option.setter("");
                args.erase(it);
            } else {
                if (it+1 != args.end()) {
                    setValue(*(it + 1));
                    args.erase(it+1);
                    args.erase(it);
                } else {
                    throw std::runtime_error("Wrong format of option " + arg);
                }
            }
        }
    }
}

std::string CommandLine::Option::optFormatHelp() const
{
    std::stringstream ss;
    if (!m_format.shortFormat.empty()) {
        ss << m_format.shortFormat;
    }
    if (!m_format.shortFormat.empty() && !m_format.longFormat.empty()) {
        ss << ", ";
    }
    if (!m_format.longFormat.empty()) {
        ss << m_format.longFormat;
    }
    return ss.str();
}

std::string CommandLine::Option::helpDesc() const
{
    std::stringstream ss;
    ss << m_option.description;
    if (m_oneOfMany.size()) {
        ss << "(one of: " << implode(m_oneOfMany, ", ") << ")";
    }
    if (!m_option.def.empty()) {
        ss << " [default: " << m_option.def << "]";
    }
    return ss.str();
}

void CommandLine::Option::setOneOfMany(const std::vector<std::string> values)
{
    m_oneOfMany = values;
}

void CommandLine::Option::setValue(const std::string& str)
{
    if (m_oneOfMany.size()) {
        std::cerr << "one of many" << std::endl;
        auto it = std::find(m_oneOfMany.begin(), m_oneOfMany.end(), str);
        if (it == m_oneOfMany.end()) {
            std::stringstream ss;
            ss << "Value '" << str << "' of option '" << m_option.format << "' should be one from ["
               << implode(m_oneOfMany, ", ") << "]";

            throw std::runtime_error(ss.str());
        }
    }
    m_option.setter(str);
}

// ===========================================================================================================

CommandLine::Option::Format::Format(const std::string& format)
{
    std::vector<std::string> tokens = fty::split(format, "|");
    if (tokens.size() == 2) { // long and short formats
        longFormat  = tokens[0];
        shortFormat = tokens[1];
    } else if (tokens.size() == 1) {
        if (tokens[0].find("--") == 0) {
            longFormat = tokens[0];
        } else {
            shortFormat = tokens[0];
        }
    } else {
        throw std::runtime_error("Wrong format of the option");
    }
}

// ===========================================================================================================

CommandLine::CommandLine(const std::string& description, std::initializer_list<OptionDef> options)
    : m_description(description)
{
    for (const auto& def : options) {
        m_options.emplace_back(std::make_unique<Option>(def));
    }
}

bool CommandLine::parse(int argc, char** argv)
{
    std::vector<std::string> args(argv+1, argv+argc);
    try {
        for (auto& opt : m_options) {
            opt->consume(args);
        }
    } catch (const std::runtime_error& err) {
        m_error = err.what();
        return false;
    }
    bool isError = false;
    for(const auto& it: args) {
        if (it.find("-") == 0) {
            isError = true;
            std::cerr << "Unknown option " << it << std::endl;
        } else {
            m_positionalArgs.push_back(it);
        }
    }
    return !isError;
}

std::string CommandLine::help() const
{
    std::stringstream ss;
    ss << m_description << std::endl;
    ss << std::endl;

    size_t len = 0;
    for (const auto& opt : m_options) {
        size_t optLen = opt->optFormatHelp().size();
        if (optLen > len) {
            len = optLen;
        }
    }

    for (const auto& opt : m_options) {
        std::string format = opt->optFormatHelp();
        ss << "  " << format << std::string(len - format.size() + 1, ' ');
        ss << opt->helpDesc() << std::endl;
    }
    return ss.str();
}

const std::vector<std::string>& CommandLine::positionalArgs() const
{
    return m_positionalArgs;
}

CommandLine::Option& CommandLine::option(const std::string& key)
{
    for (auto& opt : m_options) {
        if (opt->match(key)) {
            return *opt;
        }
    }
    throw std::runtime_error("not such option " + key);
}

const std::string& CommandLine::error() const
{
    return m_error;
}

} // namespace fty
