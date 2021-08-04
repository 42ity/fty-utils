#pragma once
#include "fty/event.h"
#include <atomic>
#include <functional>
#include <map>
#include <thread>

namespace fty {

namespace details {
    class TimersHolder;
    class RepeatableImpl;
} // namespace details

/// Simple timer implemenatation.
/// Please, do not use long term functions as timer callback
class Timer
{
public:
    Timer(const Timer& other);
    Timer& operator=(const Timer& other);

    /// Returns if timer is active
    bool isActive() const;
    /// Returns if timer is repeatable
    bool isRepeatable() const;
    /// Stops the timer
    /// @note Do not call in callback function
    void stop();

public:
    /// Finish event
    Event<> finish;

public:
    /// Creates single shot timer
    template <typename Func>
    static Timer singleShot(int msec, Func&& func);

    /// Creates single shot timer
    template <typename Rep, typename Period, typename Func>
    static Timer singleShot(const std::chrono::duration<Rep, Period>& interval, Func&& func);

    /// Creates single shot timer
    template <typename Func, typename Cls>
    static Timer singleShot(int msec, Func&& func, Cls* cls);

    /// Creates single shot timer
    template <typename Rep, typename Period, typename Func, typename Cls>
    static Timer singleShot(const std::chrono::duration<Rep, Period>& interval, Func&& func, Cls* cls);

    /// Creates repeatable timer
    template <typename Func>
    static Timer repeatable(int msec, Func&& func);

    /// Creates repeatable timer
    template <typename Rep, typename Period, typename Func>
    static Timer repeatable(const std::chrono::duration<Rep, Period>& interval, Func&& func);

    /// Creates repeatable timer
    template <typename Func, typename Cls>
    static Timer repeatable(int msec, Func&& func, Cls* cls);

    /// Creates repeatable timer
    template <typename Rep, typename Period, typename Func, typename Cls>
    static Timer repeatable(const std::chrono::duration<Rep, Period>& interval, Func&& func, Cls* cls);

private:
    Timer(uint64_t timerId);
    void triggerFinish(uint64_t timerId);

private:
    friend class details::RepeatableImpl;
    static details::TimersHolder& holder();

    Slot<uint64_t> onFinish  = {&Timer::triggerFinish, this};
    uint64_t       m_timerId = 0;
};

// =========================================================================================================================================

namespace details {
    class TimerImpl;

    class TimersHolder
    {
    public:
        TimersHolder();
        uint64_t addTimer(std::unique_ptr<TimerImpl>&& timer);

        ~TimersHolder();

        bool        isActive(uint64_t timerId) const;
        bool        isRepeatable(uint64_t timerId) const;
        void        stopTimer(uint64_t timerId);
        void        stop();
        TimerImpl*  timer(uint64_t timerId);

        Event<uint64_t> timerFinished;

    private:
        void calcNextTimeout();
        void worker();
        void removeTimer(uint64_t timerId);

    private:
        std::thread                                    m_thread;
        std::map<uint64_t, std::unique_ptr<TimerImpl>> m_timers;
        std::condition_variable                        m_cv;
        std::atomic<bool>                              m_running     = true;
        std::atomic<bool>                              m_nextChanged = false;
        std::mutex                                     m_mutex;
        std::chrono::steady_clock::time_point          m_nextTimeout;
        uint64_t                                       m_currentTimer = 0;
    };

    class TimerImpl
    {
    public:
        virtual ~TimerImpl() = default;

        TimerImpl(std::chrono::milliseconds interval)
            : m_interval(interval)
            , m_point(std::chrono::steady_clock::now())
        {
        }

        std::chrono::steady_clock::time_point nextFireTime() const
        {
            return m_point + m_interval;
        }

    protected:
        std::chrono::milliseconds             m_interval;
        std::chrono::steady_clock::time_point m_point;
    };

    class SingleShotImpl : public TimerImpl
    {
    public:
        Event<> timeout;

    private:
        SingleShotImpl(const std::chrono::milliseconds& msec, std::function<void()> onTimeout)
            : TimerImpl(msec)
            , m_function(onTimeout)
        {
            m_timeoutSlot.connect(timeout);
        }

        void onTimeout()
        {
            m_function();
        }

    private:
        friend class fty::Timer;
        Slot<>                m_timeoutSlot = {&SingleShotImpl::onTimeout, this};
        std::function<void()> m_function;
    };

    class RepeatableImpl : public TimerImpl
    {
    public:
        Event<bool&> timeout;

    private:
        RepeatableImpl(const std::chrono::milliseconds& msec, std::function<bool()> onTimeout)
            : TimerImpl(msec)
            , m_function(onTimeout)
        {
            m_timeoutSlot.connect(timeout);
        }

        void onTimeout(bool& result)
        {
            result  = m_function();
            m_point = std::chrono::steady_clock::now();
        }

    private:
        friend class fty::Timer;
        Slot<bool&>           m_timeoutSlot = {&RepeatableImpl::onTimeout, this};
        std::function<bool()> m_function;
    };
} // namespace details

// =========================================================================================================================================

details::TimersHolder::TimersHolder()
    : m_thread(&TimersHolder::worker, this)
{
    pthread_setname_np(m_thread.native_handle(), "timer");
}

uint64_t details::TimersHolder::addTimer(std::unique_ptr<TimerImpl>&& timer)
{
    static uint64_t id = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_timers.emplace(++id, std::move(timer));
        calcNextTimeout();
    }
    m_cv.notify_all();

    return id;
}

details::TimersHolder::~TimersHolder()
{
    stop();
}

bool details::TimersHolder::isActive(uint64_t timerId) const
{
    return m_timers.count(timerId) > 0;
}

void details::TimersHolder::calcNextTimeout()
{
    m_currentTimer = 0;
    m_nextTimeout  = std::chrono::steady_clock::now() + std::chrono::hours(8760);
    if (m_timers.empty()) {
        return;
    }

    for (const auto& [id, timer] : m_timers) {
        auto next = timer->nextFireTime();
        if (m_nextTimeout >= next) {
            m_nextTimeout  = next;
            m_currentTimer = id;
        }
    }
}

void details::TimersHolder::worker()
{
    m_running = true;
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_nextTimeout < std::chrono::steady_clock::now()) {
            m_nextTimeout = std::chrono::steady_clock::now()+std::chrono::milliseconds(1);
        }

        m_cv.wait_until(lock, m_nextTimeout, [&]() {
            return !m_running || m_nextChanged;
        });

        if (!m_running) {
            return;
        }

        if (m_nextChanged) {
            m_nextChanged = false;
            continue;
        }

        if (isActive(m_currentTimer)) {
            if (auto st = dynamic_cast<SingleShotImpl*>(m_timers[m_currentTimer].get())) {
                st->timeout();
                removeTimer(m_currentTimer);
            } else if (auto rt = dynamic_cast<RepeatableImpl*>(m_timers[m_currentTimer].get())) {
                bool result = false;
                rt->timeout(result);
                if (!result) {
                    removeTimer(m_currentTimer);
                }
            }
        }

        calcNextTimeout();
    }
}

void details::TimersHolder::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
        m_timers.clear();
    }
    m_cv.notify_all();
    m_thread.join();
}

void details::TimersHolder::stopTimer(uint64_t timerId)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        removeTimer(timerId);
        calcNextTimeout();
    }
    m_cv.notify_all();
}

void details::TimersHolder::removeTimer(uint64_t timerId)
{
    if (m_timers.count(timerId)) {
        // m_timers[timerId]->finish();
        m_timers.erase(timerId);
        timerFinished(std::move(timerId));
    }
}

details::TimerImpl* details::TimersHolder::timer(uint64_t timerId)
{
    if (m_timers.count(timerId)) {
        return m_timers[timerId].get();
    }
    return nullptr;
}

bool details::TimersHolder::isRepeatable(uint64_t timerId) const
{
    if (m_timers.count(timerId)) {
        return dynamic_cast<RepeatableImpl*>(m_timers.at(timerId).get()) != nullptr;
    }
    return false;
}

// =========================================================================================================================================

inline details::TimersHolder& Timer::holder()
{
    static details::TimersHolder holder;
    return holder;
}

template <typename Func>
Timer Timer::singleShot(int msec, Func&& func)
{
    return singleShot(std::chrono::milliseconds(msec), std::move(func));
}

template <typename Rep, typename Period, typename Func>
Timer Timer::singleShot(const std::chrono::duration<Rep, Period>& interval, Func&& func)
{
    std::unique_ptr<details::SingleShotImpl> ptr(new details::SingleShotImpl(interval, std::move(func)));

    Timer ret(holder().addTimer(std::move(ptr)));
    return ret;
}

template <typename Func, typename Cls>
Timer Timer::singleShot(int msec, Func&& func, Cls* cls)
{
    return singleShot(std::chrono::milliseconds(msec), std::move(func), cls);
}

template <typename Rep, typename Period, typename Func, typename Cls>
Timer Timer::singleShot(const std::chrono::duration<Rep, Period>& interval, Func&& func, Cls* cls)
{
    return singleShot(interval, [f = std::move(func), c = cls]() -> void {
        std::invoke(f, *c);
    });
}

template <typename Func>
Timer Timer::repeatable(int msec, Func&& func)
{
    return repeatable(std::chrono::milliseconds(msec), std::move(func));
}

template <typename Rep, typename Period, typename Func>
Timer Timer::repeatable(const std::chrono::duration<Rep, Period>& interval, Func&& func)
{
    std::unique_ptr<details::RepeatableImpl> ptr(new details::RepeatableImpl(interval, std::move(func)));

    Timer ret(holder().addTimer(std::move(ptr)));
    return ret;
}

template <typename Func, typename Cls>
Timer Timer::repeatable(int msec, Func&& func, Cls* cls)
{
    return repeatable(std::chrono::milliseconds(msec), std::move(func), cls);
}

template <typename Rep, typename Period, typename Func, typename Cls>
Timer Timer::repeatable(const std::chrono::duration<Rep, Period>& interval, Func&& func, Cls* cls)
{
    return repeatable(interval, [f = std::move(func), c = cls]() -> void {
        std::invoke(f, *c);
    });
}

inline bool Timer::isActive() const
{
    return holder().isActive(m_timerId);
}

inline void Timer::stop()
{
    holder().stopTimer(m_timerId);
}

inline Timer::Timer(uint64_t timerId)
    : m_timerId(timerId)
{
    onFinish.connect(holder().timerFinished);
}

inline Timer::Timer(const Timer& other)
    : m_timerId(other.m_timerId)
{
    onFinish.connect(holder().timerFinished);
}

inline Timer& Timer::operator=(const Timer& other)
{
    m_timerId = other.m_timerId;
    onFinish.connect(holder().timerFinished);
    return *this;
}

inline void Timer::triggerFinish(uint64_t timerId)
{
    if (timerId == m_timerId) {
        finish();
    }
}

inline bool Timer::isRepeatable() const
{
    return holder().isRepeatable(m_timerId);
}

// =========================================================================================================================================

} // namespace fty
