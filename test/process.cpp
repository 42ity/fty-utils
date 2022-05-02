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
#include "fty/process.h"
#include "fty/string-utils.h"
#include <catch2/catch.hpp>
#include <iostream>


#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>

/* implementation of Donal Fellows method */ 
int get_num_fds()
{
     int fd_count;
     char buf[64];
     struct dirent *dp;

     snprintf(buf, 64, "/proc/%i/fd/", getpid());

     fd_count = 0;
     DIR *dir = opendir(buf);
     while ((dp = readdir(dir)) != NULL) {
          fd_count++;
     }
     closedir(dir);
     return fd_count;
}

TEST_CASE("Process basic tests")
{
    SECTION("Run process")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("echo", {"-n", "hello"});

            if (auto ret = process.run()) {
                auto pid = process.wait();
                CHECK(process.readAllStandardOutput() == "hello");
            } else {
                FAIL(ret.error());
            }
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Std err")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("sh", {"-c", "1>&2 echo -n hello"});

            if (auto ret = process.run()) {
                auto pid = process.wait();
                CHECK("hello" == process.readAllStandardError());
            } else {
                FAIL(ret.error());
            }
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Run invalid process")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("/usr/bin/bad", {"-n", "hello"});
            auto pid     = process.run();
            CHECK(!pid.isValid());
            CHECK(!pid);
            CHECK("posix_spawnp failed with error: No such file or directory" == pid.error());
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Set environment variable")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("sh", {"-c", "export | grep MYVAR"});
            process.setEnvVar("MYVAR", "test-value");

            if (auto pid = process.run()) {
                if (auto status = process.wait()) {
                    CHECK(*status == 0);
                    CHECK(process.readAllStandardOutput() == "export MYVAR='test-value'\n");
                } else {
                    FAIL(status.error());
                }
            } else {
                FAIL(pid.error());
            }
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Write process")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("sh");
            auto pid     = process.run();
            CHECK(pid.isValid());
            CHECK(pid);
            CHECK(process.write("echo hello"));
            process.wait();
            CHECK("hello" == fty::trimmed(process.readAllStandardOutput()));
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Interrupt process")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("sh", {"-c", "sleep 2s"});

            if (auto pid = process.run()) {
                auto status = process.wait(10);
                CHECK(!status);
                CHECK("timeout" == status.error());
                process.interrupt();
                CHECK(!process.exists());
            } else {
                FAIL(pid.error());
            }
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Kill process")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("sh", {"-c", "sleep 2s"});

            if (auto pid = process.run()) {
                auto status = process.wait(10);
                CHECK(!status);
                CHECK("timeout" == status.error());
                process.kill();
                CHECK(!process.exists());
            } else {
                FAIL(pid.error());
            }
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Run process static")
    {
        auto beforefd = get_num_fds();
        {
            std::string out, err;
            auto        ret = fty::Process::run("echo", {"-n", "hello out"}, out);
            REQUIRE(ret);
            CHECK(*ret == 0);
            CHECK(out == "hello out");

            auto ret2 = fty::Process::run("sh", {"-c", "1>&2 echo -n hello err"}, out, err);
            REQUIRE(ret2);
            CHECK(*ret2 == 0);
            CHECK(out == "");
            CHECK(err == "hello err");

            auto ret3 = fty::Process::run("echo", {"-n", "hello"});
            REQUIRE(ret2);
            CHECK(*ret2 == 0);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Process finished before timeout")
    {
        auto beforefd = get_num_fds();
        {
            auto                                               process = fty::Process("sh", {"-c", "sleep 2s"});
            std::chrono::time_point<std::chrono::system_clock> start   = std::chrono::system_clock::now();

            if (auto pid = process.run()) {
                auto status = process.wait(5 * 1000);

                std::chrono::time_point<std::chrono::system_clock> stopped = std::chrono::system_clock::now();
                auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(stopped - start);

                CHECK(durationMs.count() > 2000); // check that sleep happened
                CHECK(durationMs.count() < 5000); // check that we left before timeout

                std::cout << "Duration of exception: " << durationMs.count() << std::endl;

                if (status) {
                    CHECK(*status == 0);
                } else {
                    FAIL(status.error());
                }
                CHECK(!process.exists());

            } else {
                FAIL(pid.error());
            }
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }
}

TEST_CASE("Write in process 2 time")
{
    auto beforefd = get_num_fds();
    {
        auto process = fty::Process("/bin/cat");
        auto pid     = process.run();
        CHECK(pid.isValid());
        CHECK(pid);
        CHECK(process.write("hello\n"));
        CHECK("hello" == fty::trimmed(process.readAllStandardOutput()));
        CHECK(process.write("hello2\n"));
        CHECK("hello2" == fty::trimmed(process.readAllStandardOutput()));
    }
    auto afterfd = get_num_fds();
    CHECK(beforefd == afterfd);
}

TEST_CASE("Launch 2 process at the time")
{
    SECTION("Process finished before timeout")
    {
        auto beforefd = get_num_fds();
        {
            auto process1 = fty::Process("sh", {"-c", "sleep 1s"});
            auto process2 = fty::Process("sh", {"-c", "sleep 1s"});
            auto pid1     = process1.run();
            auto pid2     = process2.run();
            CHECK(pid1);
            CHECK(pid2);
            CHECK(*pid1 != *pid2);
            CHECK(process1.wait());
            CHECK(process2.wait());
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Process finished after timeout")
    {
        auto beforefd = get_num_fds();
        {
            auto process1 = fty::Process("sh", {"-c", "sleep 1s"});
            auto process2 = fty::Process("sh", {"-c", "sleep 1s"});
            auto pid1     = process1.run();
            auto pid2     = process2.run();
            CHECK(pid1);
            CHECK(pid2);
            CHECK(*pid1 != *pid2);
            auto status1 = process1.wait(30000);
            auto status2 = process2.wait(30000);

            if (status1) {
                CHECK(*status1 == 0);
            } else {
                FAIL(status1.error());
            }

            if (status2) {
                CHECK(*status2 == 0);
            } else {
                FAIL(status2.error());
            }
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }
}

TEST_CASE("Launch 2 process at the time with launcher in separeted thread")
{
    using namespace std::chrono_literals;

    auto funcWithoutTimeout = []() {
        auto process = fty::Process("sh", {"-c", "sleep 3s"});
        auto pid     = process.run();
        CHECK(pid);
        std::this_thread::sleep_for(1s);
        CHECK(process.wait());
    };

    auto funcWithTimeout = [](uint64_t timeout) {
        auto process = fty::Process("sh", {"-c", "sleep 3s"});
        auto pid     = process.run();
        CHECK(pid);
        std::this_thread::sleep_for(1s);
        CHECK(process.wait(timeout));
    };

    SECTION("Process finished before timeout")
    {
        auto beforefd = get_num_fds();
        {
            std::thread t1(funcWithoutTimeout);
            std::thread t2(funcWithoutTimeout);
            t1.join();
            t2.join();
            CHECK(true);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Process finished after timeout")
    {
        auto beforefd = get_num_fds();
        {
            std::thread t1(funcWithTimeout, 50000);
            std::thread t2(funcWithTimeout, 50000);
            t1.join();
            t2.join();
            CHECK(true);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }
}

TEST_CASE("Process with Huge data")
{
    auto beforefd = get_num_fds();
    {
        auto process = fty::Process("dpkg", {"-l"});
        auto pid     = process.run();
        CHECK(pid.isValid());
        CHECK(pid);
        //CHECK(process.readAllStandardOutput().size());
    }
    auto afterfd = get_num_fds();
    CHECK(beforefd == afterfd);
}

TEST_CASE("Process with More data than pipe support")
{
    auto beforefd = get_num_fds();
    {
        auto process = fty::Process("sh", {"-c","counter=68000; while [ $counter -gt 0 ]; do printf \"X\"; counter=$(expr $counter - 1); done; echo \"\""});
        auto pid     = process.run();
        CHECK(pid.isValid());
        CHECK(*pid);
        process.wait();
        CHECK (process.readAllStandardOutput().length() == 68001);
    }
    auto afterfd = get_num_fds();
    CHECK(beforefd == afterfd);
}
