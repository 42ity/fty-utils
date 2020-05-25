#include "fty/event.h"
#include <catch2/catch.hpp>
#include <condition_variable>
#include <iostream>
#include <thread>

static int memCall   = 0;
static int lamCall   = 0;
static int scopeCall = 0;
static int statCall  = 0;

class Consumer
{
public:
    Consumer(fty::Event<int>& sig)
    {
        slot.connect(sig);
    }

    void slot1(int val)
    {
        ++memCall;
        CHECK((val == 42 || val == 112));
    }

private:
    fty::Slot<int> slot = {&Consumer::slot1, this};
};

static void func(int val)
{
    ++statCall;
    CHECK((val == 42 || val == 112));
}

TEST_CASE("Event")
{
    fty::Event<int> sig;

    Consumer cons(sig);

    fty::Slot<int> slot2([](int val) {
        ++lamCall;
        CHECK((val == 42 || val == 112));
    });
    slot2.connect(sig);

    fty::Slot<int> slot4(&func);
    sig.connect(slot4);

    {
        fty::Slot<int> slot3([](int val) {
            ++scopeCall;
            CHECK(val == 42);
        });
        sig.connect(slot3);
        sig(42);
    }

    sig(112);

    CHECK(memCall == 2);
    CHECK(lamCall == 2);
    CHECK(statCall == 2);
    CHECK(scopeCall == 1);

    auto sig1 = std::move(sig);
    sig1(112);
    CHECK(memCall == 3);
    CHECK(lamCall == 3);
    CHECK(statCall == 3);
    CHECK(scopeCall == 1);
}

TEST_CASE("Event thread")
{
    fty::Event<int&> sig;

    std::condition_variable var;
    std::mutex              mutex;
    bool                    fired = false;
    bool                    ready = false;
    int                     val   = 0;

    std::thread th2([&]() {
        fty::Slot<int&> slot([&](int& val) {
            val = 42;
        });
        sig.connect(slot);

        {
            std::lock_guard<std::mutex> lk(mutex);
            ready = true;
        }
        var.notify_one();

        std::unique_lock<std::mutex> lk(mutex);
        var.wait(lk, [&] {
            return fired;
        });
    });

    std::thread th1([&]() {
        {
            std::unique_lock<std::mutex> lk(mutex);
            var.wait(lk, [&] {
                return ready;
            });

            sig(val);
            fired = true;
        }
        var.notify_one();
    });

    th2.join();
    th1.join();

    CHECK(42 == val);
}
