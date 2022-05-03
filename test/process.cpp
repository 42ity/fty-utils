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

            //check we do not have dataif process is not running
            CHECK(process.readAllStandardOutput() == "");

            if (auto pid = process.run()) {
                CHECK(process.readAllStandardOutput() == "export MYVAR='test-value'\n");
                if (auto status = process.wait()) {
                    CHECK(*status == 0);
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
            process.closeWriteChannel();
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
                CHECK(status.error() == "timeout");

                auto statusInterrupt = process.interrupt();
                CHECK(!statusInterrupt);
                CHECK(statusInterrupt.error() == "Interrupt");

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
                CHECK(status.error() == "timeout");

                auto statusKill = process.kill();
                CHECK(!statusKill);
                CHECK(statusKill.error() == "Killed");
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
            auto process1 = fty::Process("sh", {"-c", "sleep 2s"});
            auto process2 = fty::Process("sh", {"-c", "sleep 2s"});
            auto pid1     = process1.run();
            auto pid2     = process2.run();
            CHECK(pid1);
            CHECK(pid2);
            CHECK(*pid1 != *pid2);
            auto status1 = process1.wait(300);
            auto status2 = process2.wait(300);

            if (!status1) {
                CHECK(status1.error() == "timeout");
            } else {
                FAIL(status1.error());
            }

            if (!status2) {
                CHECK(status2.error() == "timeout");
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

    auto func = [](uint64_t timeout, bool shouldTimeOut = false) {
        auto process = fty::Process("sh", {"-c", "sleep 3s"});
        auto pid     = process.run();
        CHECK(pid);
        std::this_thread::sleep_for(1s);

        auto status = process.wait(timeout);

        if(!shouldTimeOut) {
            CHECK(status);
        } else {
            if (!status) {
                CHECK(status.error() == "timeout");
            } else {
                FAIL("Process finished and it was not expected");
            }
        }
        
    };

    SECTION("Process finished before timeout")
    {
        auto beforefd = get_num_fds();
        {
            //func takes 3 seconds to execute a process
            std::thread t1(func, 6000); //wait 6s
            std::thread t2(func, 6000); //wait 6s
            t1.join();
            t2.join();
            CHECK(true);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Process finished after timeout")
    {
        //func takes 3 seconds to execute a process
        auto beforefd = get_num_fds();
        {
            std::thread t1(func, 500, true); //wait 500ms
            std::thread t2(func, 500, true); //wait 500ms
            t1.join();
            t2.join();
            CHECK(true);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }
}

TEST_CASE("Test hard kill process")
{
    auto beforefd = get_num_fds();
    {
        auto process = fty::Process("bash", {"-c", "sleep 10"});
        auto pid     = process.run();
        CHECK(pid.isValid());
        CHECK(pid);

        kill(*pid, SIGKILL);
        auto status = process.wait(1000);

        CHECK(!status);
        CHECK(status.error() == "Killed (core dumped)" || status.error() == "Killed");
    }
    auto afterfd = get_num_fds();
    CHECK(beforefd == afterfd);
}

TEST_CASE("Test segfault process")
{
    auto beforefd = get_num_fds();
    {
        auto process = fty::Process("bash");
        auto pid     = process.run();
        CHECK(pid.isValid());
        CHECK(pid);

        kill(*pid, SIGSEGV);
        auto status = process.wait(1000);
        if(status) {
            FAIL("Process did not detect segfault");
        } else {
            CHECK(status.error() == "Segmentation fault (core dumped)" || status.error() == "Segmentation fault");
        }
        
    }
    auto afterfd = get_num_fds();
    CHECK(beforefd == afterfd);
}

TEST_CASE("Process with lot of data in out")
{
    SECTION("Use args to launch process")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("bash", {"-c", "for I in {1..68000}; do printf \"X\";  done;"});
            auto pid     = process.run();
            CHECK(pid.isValid());
            CHECK(*pid);
            auto status = process.wait();
            CHECK(status);
            CHECK(*status == 0);
            CHECK (process.readAllStandardOutput().length() == 68000);
            CHECK (process.readAllStandardError().length() == 0);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Use stdin to launch process")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("bash");
            auto pid     = process.run();
            process.write("for I in {1..68000}; do printf \"X\";  done;");
            CHECK(pid.isValid());
            CHECK(*pid);
            auto status = process.wait();
            CHECK(status);
            CHECK(*status == 0);
            CHECK (process.readAllStandardError().length() == 0);
            CHECK (process.readAllStandardOutput().length() == 68000);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Ignore capture")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("bash", {"-c", "for I in {1..68000}; do printf \"X\";  done;"}, fty::Capture::None);
            auto pid     = process.run();
            CHECK(pid.isValid());
            CHECK(*pid);
            auto status = process.wait();
            CHECK(status);
            CHECK(*status == 0);
            CHECK (process.readAllStandardOutput().length() == 0);
            CHECK (process.readAllStandardError().length() == 0);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }

    SECTION("Test on stderr")
    {
        auto beforefd = get_num_fds();
        {
            auto process = fty::Process("bash", {"-c", "for I in {1..68000}; do printf \"X\" >&2;  done;"});
            auto pid     = process.run();
            CHECK(pid.isValid());
            CHECK(*pid);
            auto status = process.wait();
            CHECK(status);
            CHECK(*status == 0);
            CHECK (process.readAllStandardOutput().length() == 0);
            CHECK (process.readAllStandardError().length() == 68000);
        }
        auto afterfd = get_num_fds();
        CHECK(beforefd == afterfd);
    }
}