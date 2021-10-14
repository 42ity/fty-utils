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
#include "fty/thread-pool.h"
#include "fty/string-utils.h"
#include <catch2/catch.hpp>

TEST_CASE("ThreadPool")
{
    auto func = [&](int num) {
      std::this_thread::sleep_for(std::chrono::seconds(1 * num));
      std::cout << "Stop func" << num << std::endl;
    };

    fty::ThreadPool pool;
    for (int i=0; i < 10; i++) {
      pool.pushWorker(func, i);
    }
    std::cout << "" << std::endl;
    std::cout << "Before stop" << std::endl;
    pool.stop();
    std::cout << "After stop" << std::endl;
}

