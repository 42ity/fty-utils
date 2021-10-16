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
#include "fty/string-utils.h"
#include "fty/thread-pool.h"
#include <catch2/catch.hpp>


TEST_CASE("ThreadPool fixed workers")
{
    std::atomic_int started  = 0;
    std::atomic_int executed = 0;

    auto func = [&](int num) {
        started++;
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        executed++;
        std::cout << std::this_thread::get_id() << " Stop task num: " << num << std::endl;
    };

    {
        // create a pool of 2 threads
        fty::ThreadPool pool(2);
        std::cout << "\n>> Pool with 2 workers has been create" << std::endl;

        CHECK(pool.getCountAllocatedThreads() == 2);
        CHECK(pool.getCountPendingTasks() == 0);

        // push 5 tasks
        for (int i = 0; i < 5; i++) {
            pool.pushWorker(func, i);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));

        CHECK(pool.getCountAllocatedThreads() == 2);
        CHECK(pool.getCountPendingTasks() == 3);


        std::cout << ">> Descructor of the pool will be called" << std::endl;
    }

    std::cout << ">> Pool has been stopped\n" << std::endl;

    CHECK(started == 2);
    CHECK(executed == 2);
}

TEST_CASE("ThreadPool fixed workers - exception in task")
{
    class MyException : public std::runtime_error
    {
    public:
        inline MyException(): std::runtime_error("Test"){};
    };

    auto func = [&](int num) {
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        throw MyException();
    };

    {
        // create a pool
        fty::ThreadPool pool;
        std::cout << "\n>> Pool with optimized workers has been create" << std::endl;

        CHECK(pool.getCountAllocatedThreads() == std::thread::hardware_concurrency() - 1);
        CHECK(pool.getCountPendingTasks() == 0);

        // push a task
        auto myTask = pool.pushWorker(func, 0);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        CHECK(pool.getCountAllocatedThreads() == std::thread::hardware_concurrency() - 1);
        CHECK(pool.getCountPendingTasks() == 0);
        CHECK(pool.getCountActiveTasks() == 0);

        CHECK(myTask->getException() != nullptr);

        REQUIRE_THROWS_AS(std::rethrow_exception(myTask->getException()), MyException);
        std::cout << ">> Descructor of the pool will be called" << std::endl;
    }

    std::cout << ">> Pool has been stopped\n" << std::endl;
}

TEST_CASE("ThreadPool fixed workers with normal stop")
{
    std::atomic_int started  = 0;
    std::atomic_int executed = 0;

    auto func = [&](int num) {
        started++;
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        executed++;
        std::cout << std::this_thread::get_id() << " Stop task num: " << num << std::endl;
    };

    // create a pool of 2 threads
    fty::ThreadPool pool(2);
    std::cout << "\n>> Pool with 2 workers has been create" << std::endl;

    CHECK(pool.getCountAllocatedThreads() == 2);
    CHECK(pool.getCountPendingTasks() == 0);
    CHECK(pool.getCountActiveTasks() == 0);

    // push 5 tasks
    for (int i = 0; i < 5; i++) {
        pool.pushWorker(func, i);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    CHECK(pool.getCountAllocatedThreads() == 2);
    CHECK(pool.getCountActiveTasks() == 2);
    CHECK(pool.getCountPendingTasks() == 3);

    std::cout << ">> We stop the pool after one second" << std::endl;
    pool.stop();

    CHECK(pool.getCountActiveTasks() == 0);
    CHECK(pool.getCountPendingTasks() == 0);

    std::cout << ">> Pool has been stopped\n" << std::endl;

    CHECK(started == 5);
    CHECK(executed == 5);
}

TEST_CASE("ThreadPool fixed workers immediat stop")
{
    std::atomic_int started  = 0;
    std::atomic_int executed = 0;

    auto func = [&](int num) {
        started++;
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        executed++;
        std::cout << std::this_thread::get_id() << " Stop task num: " << num << std::endl;
    };

    // create a pool of 2 threads
    fty::ThreadPool pool(2);
    std::cout << "\n>> Pool with 2 workers has been create" << std::endl;

    CHECK(pool.getCountAllocatedThreads() == 2);
    CHECK(pool.getCountPendingTasks() == 0);
    CHECK(pool.getCountActiveTasks() == 0);

    // push 5 tasks
    for (int i = 0; i < 5; i++) {
        pool.pushWorker(func, i);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    CHECK(pool.getCountAllocatedThreads() == 2);
    CHECK(pool.getCountActiveTasks() == 2);
    CHECK(pool.getCountPendingTasks() == 3);

    std::cout << ">> We stop immediatly the pool after one second (only 2 task should have started)" << std::endl;
    pool.stop(fty::ThreadPool::Stop::Immedialy);

    CHECK(pool.getCountActiveTasks() == 0);
    CHECK(pool.getCountPendingTasks() == 0);

    std::cout << ">> Pool has been stopped\n" << std::endl;

    CHECK(started == 2);
    CHECK(executed == 2);
}

TEST_CASE("ThreadPool fixed workers cancel stop")
{
    std::atomic_int started  = 0;
    std::atomic_int executed = 0;

    auto func = [&](int num) {
        started++;
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        executed++;
        std::cout << std::this_thread::get_id() << " Stop task num: " << num << std::endl;
    };

    // create a pool of 2 threads
    fty::ThreadPool pool(2);
    std::cout << "\n>> Pool with 2 workers has been create" << std::endl;

    CHECK(pool.getCountAllocatedThreads() == 2);
    CHECK(pool.getCountPendingTasks() == 0);

    // push 5 tasks
    for (int i = 0; i < 5; i++) {
        pool.pushWorker(func, i);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << ">> We Cancel the pool after one second (only 2 task should have started, 0 will finished)" << std::endl;
    pool.stop(fty::ThreadPool::Stop::Cancel);
    std::cout << ">> Pool has been stopped\n" << std::endl;

    CHECK(started == 2);
    CHECK(executed == 0);
}

TEST_CASE("ThreadPool dynamic workers - need more allocation")
{
    std::atomic_int started  = 0;
    std::atomic_int executed = 0;

    auto func = [&](int num) {
        started++;
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        executed++;
        std::cout << std::this_thread::get_id() << " Stop task num: " << num << std::endl;
    };

    // Create a pool
    fty::ThreadPool pool(1, 3);
    std::cout << "\n>> Pool with dynamic workers from 1 to 3 has been create" << std::endl;

    CHECK(pool.getCountAllocatedThreads() == 1);
    CHECK(pool.getCountPendingTasks() == 0);
    CHECK(pool.getCountActiveTasks() == 0);

    // Push 5 tasks
    for (int i = 0; i < 5; i++) {
        pool.pushWorker(func, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    CHECK(pool.getCountAllocatedThreads() == 3);
    CHECK(pool.getCountActiveTasks() == 3);
    CHECK(pool.getCountPendingTasks() == 2);

    std::cout << ">> Wait 10 seconds" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    CHECK(pool.getCountAllocatedThreads() == 1);
    CHECK(pool.getCountPendingTasks() == 0);
    CHECK(pool.getCountActiveTasks() == 0);

    CHECK(started == 5);
    CHECK(executed == 5);

    std::cout << ">> Finished" << std::endl;
}

TEST_CASE("ThreadPool dynamic workers - no need for more allocation")
{
    std::atomic_int started  = 0;
    std::atomic_int executed = 0;

    auto func = [&](int num) {
        started++;
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        executed++;
        std::cout << std::this_thread::get_id() << " Stop task num: " << num << std::endl;
    };

    // Create a pool
    fty::ThreadPool pool(5, 7);
    std::cout << "\n>> Pool with dynamic workers from 5 to 7 has been create" << std::endl;

    CHECK(pool.getCountAllocatedThreads() == 5);
    CHECK(pool.getCountPendingTasks() == 0);
    CHECK(pool.getCountActiveTasks() == 0);

    // Push 5 tasks
    for (int i = 0; i < 5; i++) {
        pool.pushWorker(func, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    CHECK(pool.getCountAllocatedThreads() == 5);
    CHECK(pool.getCountActiveTasks() == 5);
    CHECK(pool.getCountPendingTasks() == 0);

    std::cout << ">> Wait 3 seconds" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    CHECK(pool.getCountAllocatedThreads() == 5);
    CHECK(pool.getCountPendingTasks() == 0);
    CHECK(pool.getCountActiveTasks() == 0);

    CHECK(started == 5);
    CHECK(executed == 5);

    std::cout << ">> Finished" << std::endl;
}

TEST_CASE("ThreadPool dynamic workers stop")
{
    std::atomic_int started  = 0;
    std::atomic_int executed = 0;

    auto func = [&](int num) {
        started++;
        std::cout << std::this_thread::get_id() << " Start task num: " << num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        executed++;
        std::cout << std::this_thread::get_id() << " Stop task num: " << num << std::endl;
    };

    // Create a pool
    fty::ThreadPool pool(1, 3);
    std::cout << "\n>> Pool with dynamic workers from 1 to 3 has been create" << std::endl;

    CHECK(pool.getCountAllocatedThreads() == 1);
    CHECK(pool.getCountPendingTasks() == 0);

    // Push 5 tasks
    for (int i = 0; i < 5; i++) {
        pool.pushWorker(func, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    CHECK(pool.getCountAllocatedThreads() == 3);
    CHECK(pool.getCountPendingTasks() == 2);

    std::cout << ">> Stop the pool" << std::endl;

    pool.stop();

    CHECK(pool.getCountPendingTasks() == 0);
    CHECK(pool.getCountActiveTasks() == 0);

    CHECK(started == 5);
    CHECK(executed == 5);

    std::cout << ">> Finished" << std::endl;
}
