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
#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_DISABLE_EXCEPTIONS
#include <catch2/catch.hpp>

//add all the include here, so that they appear in coverage report
#include "fty/command-line.h"
#include "fty/convert.h"
#include "fty/event.h"
#include "fty/expected.h"
#include "fty/flags.h"
#include "fty/process.h"
#include "fty/string-utils.h"
#include "fty/thread-pool.h"
#include "fty/timer.h"
#include "fty/traits.h"
#include "fty/translate.h"
