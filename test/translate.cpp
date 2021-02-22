#include <catch2/catch.hpp>
#include "fty/translate.h"
#include <sstream>
#include <iostream>

TEST_CASE("Translate/simple")
{
    auto tr = fty::tr("this is an ex-parrot");
    CHECK("this is an ex-parrot" == tr.toString());
}

TEST_CASE("Translate/positional")
{
    auto tr = fty::tr("parrot: {} {}").format("norwegian", "blue");
    CHECK("parrot: norwegian blue" == tr.toString());
}

TEST_CASE("Translate/indexed")
{
    auto tr = fty::tr("parrot: {1} {0}").format("norwegian", "blue");
    CHECK("parrot: blue norwegian" == tr.toString());
}

TEST_CASE("Translate/named")
{
    using namespace fmt::literals;
    auto tr = fty::tr("parrot is {state}").format("state"_a = "dead");
    CHECK("parrot is dead" == tr.toString());
}

TEST_CASE("Translate/hex")
{
    auto tr = fty::tr("parrot is {:x}{:x}").format(222, 173);
    CHECK("parrot is dead" == tr.toString());
}

TEST_CASE("Translate/literal")
{
    auto tr = "parrot: {} {}"_tr.format("norwegian", "blue");
    CHECK("parrot: norwegian blue" == tr.toString());
}

TEST_CASE("Translate/streaming")
{
    std::stringstream ss;
    ss << "parrot: {} {}"_tr.format("norwegian", "blue");
}

TEST_CASE("Translate/concatination")
{
    {
        std::string s = "s" + "parrot"_tr;
        CHECK("sparrot" == s);
    }
    {
        std::string s = "parrot"_tr + "s";
        CHECK("parrots" == s);
    }
}

TEST_CASE("Translate/translate")
{
    {
        auto tr = "parrot is {}"_tr.format("dead"_tr);
        CHECK("parrot is dead" == tr.toString());
    }
    {
        auto tr = "parrot is {}"_tr.format("dead as {}"_tr.format("dead parrot"));
        CHECK("parrot is dead as dead parrot" == tr.toString());
    }
}

TEST_CASE("Translate/lifetime")
{
    {
        fty::Translate trans("parrot is {}");
        {
            std::string val("dead");
            trans.format(val);
        }
        CHECK("parrot is dead" == trans.toString());
    }
    {
        fty::Translate trans("parrot is {}");
        {
            std::string val("dead");
            trans.format(val.c_str());
        }
        CHECK("parrot is dead" == trans.toString());
    }
}

