#include <catch2/catch.hpp>
#include <fty/timer.h>
#include <chrono>

TEST_CASE("Timer")
{
    using namespace std::literals::chrono_literals;

    SECTION("Simple timer")
    {
        bool run = false;
        auto t = fty::Timer::singleShot(500, [&](){
            run = true;
        });
        CHECK(t.isActive());
        t.finish.wait();
        CHECK(run);
    }

    SECTION("Stop simple timer")
    {
        bool run = false;
        fty::Timer t = fty::Timer::singleShot(500ms, [&](){
            run = true;
        });
        CHECK(t.isActive());
        CHECK(!t.isRepeatable());
        std::this_thread::sleep_for(100ms);
        t.stop();
        t.finish.wait();
        CHECK(!run);
    }

    SECTION("Member function")
    {
        struct Listen
        {
            void onTimeout()
            {
                m_runned = true;
            }
            bool m_runned = false;
        };

        Listen lst;
        auto t = fty::Timer::singleShot(100ms, &Listen::onTimeout, &lst);
        CHECK(t.isActive());
        t.finish.wait();
        CHECK(lst.m_runned);
    }

    SECTION("Repeatable timer")
    {
        int count = 0;
        auto start = std::chrono::steady_clock::now();
        fty::Timer t = fty::Timer::repeatable(100ms, [&](){
            ++count;
            std::cerr << "tick " << count << std::endl;
            std::cerr << "elapsed " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << std::endl;
            return count != 5;
        });
        CHECK(t.isActive());
        CHECK(t.isRepeatable());
        t.finish.wait();
        CHECK(count == 5);
    }
}
