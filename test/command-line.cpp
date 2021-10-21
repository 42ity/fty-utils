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
#include "fty/command-line.h"
#include <catch2/catch.hpp>
#include <iostream>


TEST_CASE("Command line help")
{
    bool        help         = false;
    bool        optionBool   = false;
    std::string optionString = "Default value";

    // clang-format off
    fty::CommandLine cmd("Test command line", {
        {"--bool",   optionBool,    "Option bool"},
        {"--string", optionString,  "Option string"},
        {"--help",   help,          "Show this help"}
    });

    CHECK(cmd.help() ==
R"(Test command line

  --bool   Option bool
  --string Option string [default: Default value]
  --help   Show this help
)");
    // clang-format on
}


TEST_CASE("Command line without arg")
{
    bool        help         = false;
    bool        optionBool   = false;
    std::string optionString = "Default value";

    // clang-format off
    fty::CommandLine cmd("Test command line", {
        {"--bool",   optionBool,    "Option bool"},
        {"--string", optionString,  "Option string"},
        {"--help",   help,          "Show this help"}
    });

        
    char  arg0[]  = "./test";
    char* argv1[] = {arg0};

    CHECK(cmd.parse(1, argv1));

    CHECK(cmd.positionalArgs().size() == 0);

    CHECK(help == false);
    CHECK(optionBool == false);
    CHECK(optionString == "Default value");

    help         = true;
    optionBool   = true;
    optionString = "Default value";

    CHECK(cmd.parse(1, argv1));

    CHECK(help == true);
    CHECK(optionBool == true);
    CHECK(optionString == "Default value");
}

TEST_CASE("Command line with arg")
{
    bool        help         = false;
    bool        optionBool   = false;
    std::string optionString = "Default value";

    // clang-format off
    fty::CommandLine cmd("Test command line", {
        {"--bool",   optionBool,    "Option bool"},
        {"--string", optionString,  "Option string"},
        {"--help",   help,          "Show this help"}
    });

        
    char  arg0[]  = "./test";
    char  arg1[]  = "--bool";
    char  arg2[]  = "--string=Test string";
    char  arg3[]  = "--help";
    char* argv1[] = {arg0, arg1, arg2, arg3};

    CHECK(cmd.parse(4, argv1));

    CHECK(cmd.positionalArgs().size() == 0);

    CHECK(cmd.error() == "");

    CHECK(help == true);
    CHECK(optionBool == true);
    CHECK(optionString == "Test string");
}

TEST_CASE("Argument parse")
{
    bool        help         = false;
    bool        optionBool   = false;
    std::string optionString = "Default value";

    // clang-format off
    fty::CommandLine cmd("Test command line", {
        {"--bool",   optionBool,    "Option bool"},
        {"--string", optionString,  "Option string"},
        {"--help",   help,          "Show this help"}
    });


    char  arg0[]  = "./test";
    char  arg1[]  = "hello";
    char  arg2[]  = "--bool";
    char  arg3[]  = "--string=Test string";
    char  arg4[]  = "--help";
    char  arg5[]  = "end";
    char* argv1[] = {arg0, arg1, arg2, arg3, arg4, arg5};

    CHECK(cmd.parse(6, argv1));

    CHECK(cmd.positionalArgs().size() == 2);

    CHECK(cmd.error() == "");
}
