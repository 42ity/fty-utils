#include "fty/expected.h"
#include <catch2/catch.hpp>

struct St
{
    St()          = default;
    St(const St&) = delete;
    St& operator=(const St&) = delete;
    St(St&&)                 = default;

    bool func()
    {
        return true;
    }
};

TEST_CASE("Expected")
{
    SECTION("Expected")
    {
        auto it = fty::Expected<int>(32);
        CHECK(it);
        CHECK(32 == *it);
    }

    SECTION("Unexpected")
    {
        fty::Expected<int> it = fty::unexpected("wrong");
        CHECK(!it);
        CHECK("wrong" == it.error());
    }

    SECTION("Return values")
    {
        auto func = []() -> fty::Expected<St> {
            return St();
        };

        auto func2 = []() -> fty::Expected<St> {
            return fty::unexpected("wrong");
        };

        fty::Expected<St> st = func();
        CHECK(st);
        CHECK(st->func());
        CHECK((*st).func());

        fty::Expected<St> ust = func2();
        CHECK(!ust);
        CHECK("wrong" == ust.error());
    }

    SECTION("Return streamed unexpected")
    {
        auto func = []() -> fty::Expected<St> {
            return fty::unexpected() << "wrong " << 42;
        };

        fty::Expected<St> st = func();
        CHECK(!st);
        CHECK("wrong 42" == st.error());
    }
}
