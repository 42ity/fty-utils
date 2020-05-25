#include "fty/convert.h"
#include <catch2/catch.hpp>

using namespace Catch::literals;

TEST_CASE("Convert")
{
    CHECK("11" == fty::convert<std::string>(11));
    CHECK("22.22" == fty::convert<std::string>(22.22));
    CHECK("true" == fty::convert<std::string>(true));
    CHECK("false" == fty::convert<std::string>(false));
    CHECK("11" == fty::convert<std::string>(11ll));
    CHECK("11" == fty::convert<std::string>(11ul));
    CHECK("str" == fty::convert<std::string>("str"));

    CHECK(32 == fty::convert<int>(32.222));
    CHECK(1 == fty::convert<int>(true));

    CHECK(42 == fty::convert<int>("42"));
    CHECK(true == fty::convert<bool>("true"));
    CHECK(false == fty::convert<bool>("false"));
    CHECK(42.22_a == fty::convert<float>("42.22"));
    CHECK(42.22_a == fty::convert<double>("42.22"));
}

enum class Test
{
    One,
    Two
};

namespace fty {
template<>
std::string convert(const Test& test)
{
    switch (test) {
    case Test::One: return "One";
    case Test::Two: return "Two";
    }
}

template<>
Test convert(const std::string& test)
{
    std::string_view view(test);
    if (view == "One") {
        return Test::One;
    } else if (view == "Two"){
        return Test::Two;
    }
    return {};
}

}

TEST_CASE("Convert custom")
{
    CHECK("Two" == fty::convert<std::string>(Test::Two));
    CHECK("One" == fty::convert<std::string>(Test::One));

    CHECK(Test::Two == fty::convert<Test>(std::string("Two")));
    CHECK(Test::One == fty::convert<Test>(std::string("One")));

    CHECK(0 == fty::convert<int>(Test::One));
    CHECK(1 == fty::convert<int>(Test::Two));
}
