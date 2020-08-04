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

    SECTION("Void expected")
    {
        auto it = fty::Expected<void>();
        CHECK(it);

        auto func = []() -> fty::Expected<void> {
            return {};
        };
        auto func2 = []() -> fty::Expected<void> {
            return fty::unexpected("some error");
        };
        CHECK(func());
        auto res = func2();
        CHECK(!res);
        CHECK("some error" == res.error());
    }
}
