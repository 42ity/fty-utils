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
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fty/event.h>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace fty {

// ===========================================================================================================

class ITask
{
public:
    fty::Event<> started;
    fty::Event<> stopped;

    virtual ~ITask() = default;

public:
    virtual void operator()() = 0;
};

// ===========================================================================================================

template <typename T>
class Task : public ITask
{
public:
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&&)  = default;
    Task& operator=(Task&&) = default;

protected:
    Task();

private:
    friend class ThreadPool;
};

// ===========================================================================================================

namespace details {

    class PoolWatcher
    {
    public:
        template <typename Func>
        PoolWatcher(Func&& clearFunc);
        ~PoolWatcher();

        void clear(std::thread::id id);
        void stop();

    private:
        void run();

    private:
        std::thread                          m_thread;
        bool                                 m_stop = false;
        std::mutex                           m_mutex;
        std::condition_variable              m_cv;
        std::optional<std::thread::id>       m_toClear;
        std::function<void(std::thread::id)> m_clearFunc;
    };

    class GenericTask : public Task<GenericTask>
    {
    public:
        template <typename Func, typename... Args>
        GenericTask(Func&& func, Args&&... args)
            : m_func([f = std::move(func), cargs = std::make_tuple(std::forward<Args>(args)...)]() {
                std::apply(std::move(f), std::move(cargs));
            })
        {
        }

        void operator()() override
        {
            m_func();
        }

    private:
        std::function<void()> m_func;
    };

} // namespace details

// ===========================================================================================================

class ThreadPool
{
public:
    enum class Stop
    {
        WaitForQueue,
        Immedialy
    };

public:
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    static void stop(Stop mode = Stop::WaitForQueue);

    static void init(size_t numThreads = std::thread::hardware_concurrency() - 1);

public:
    template <typename T, typename... Args>
    static ITask& pushWorker(Args&&... args);

    template <typename Func, typename... Args>
    static ITask& pushWorker(Func&& fnc, Args&&... args);

private:
    ThreadPool();
    void               create(size_t numThreads);
    void               allocThread();
    static ThreadPool& instance();

private:
    std::vector<std::thread>           m_threads;
    std::mutex                         m_mutex;
    std::condition_variable            m_cv;
    bool                               m_stop = false;
    std::deque<std::shared_ptr<ITask>> m_tasks;
    std::atomic<size_t>                m_useCount = 0;
    details::PoolWatcher               m_watcher;
    size_t                             m_minNumThreads = 0;
};

// ===========================================================================================================


template <typename T>
Task<T>::Task() = default;

// ===========================================================================================================

inline ThreadPool::ThreadPool()
    : m_watcher([&](std::thread::id id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto iter = m_threads.begin(); iter != m_threads.end(); ++iter) {
            if (iter->get_id() == id) {
                iter->join();
                m_threads.erase(iter);
                break;
            }
        }
    })
{
}

inline ThreadPool::~ThreadPool()
{
    stop(Stop::Immedialy);
}

inline void ThreadPool::stop(Stop mode)
{
    auto& inst = instance();
    if (!inst.m_stop) {
        inst.m_watcher.stop();
        {
            if (mode == Stop::WaitForQueue) {
                std::unique_lock<std::mutex> lock(inst.m_mutex, std::defer_lock);
                inst.m_cv.wait(lock, [&]() {
                    return inst.m_tasks.empty();
                });
            }

            std::lock_guard<std::mutex> lock(inst.m_mutex);
            inst.m_stop = true;
        }
        inst.m_cv.notify_all();

        for (std::thread& thread : inst.m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
}

inline void ThreadPool::create(size_t numThreads)
{
    if (!m_threads.empty()) {
        return;
    }

    m_minNumThreads = numThreads;

    for (size_t i = 0; i < numThreads; ++i) {
        allocThread();
    }
}

inline void ThreadPool::allocThread()
{
    using namespace std::chrono_literals;

    m_threads.emplace_back(std::thread([&]() {
        while (true) {
            std::shared_ptr<ITask> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_cv.wait_for(lock, 1s, [&]() {
                    return !m_tasks.empty() || m_stop;
                });

                if (!m_stop && m_tasks.empty() && m_threads.size() > m_minNumThreads) {
                    m_watcher.clear(std::this_thread::get_id());
                    return;
                }

                if (m_stop) {
                    return;
                }

                if (!m_tasks.empty()) {
                    task = std::move(m_tasks.front());
                    m_tasks.pop_front();
                }
            }
            m_cv.notify_all();
            if (task) {
                ++m_useCount;
                task->started();
                (*task)();
                task->stopped();
                --m_useCount;
            }
        }
    }));
}

inline void ThreadPool::init(size_t numThreads)
{
    auto& pool = instance();
    pool.create(numThreads);
}

inline ThreadPool& ThreadPool::instance()
{
    static ThreadPool inst;
    return inst;
}

template <typename T, typename... Args>
ITask& ThreadPool::pushWorker(Args&&... args)
{
    auto&              inst = instance();
    std::shared_ptr<T> task;
    {
        std::lock_guard<std::mutex> lock(inst.m_mutex);
        if (inst.m_tasks.size() >= inst.m_threads.size()) {
            inst.allocThread();
        }
        task = std::make_shared<T>(std::forward<Args>(args)...);
        inst.m_tasks.emplace_back(task);
    }
    inst.m_cv.notify_all();
    return *task;
}

template <typename Func, typename... Args>
ITask& ThreadPool::pushWorker(Func&& fnc, Args&&... args)
{
    auto&                                 inst = instance();
    std::shared_ptr<details::GenericTask> task;
    {
        std::lock_guard<std::mutex> lock(inst.m_mutex);
        if (inst.m_tasks.size() >= inst.m_threads.size()) {
            inst.allocThread();
        }
        task = std::make_shared<details::GenericTask>(std::move(fnc), std::forward<Args>(args)...);
        inst.m_tasks.emplace_back(task);
    }
    inst.m_cv.notify_all();
    return *task;
}

// ===========================================================================================================

template <typename Func>
details::PoolWatcher::PoolWatcher(Func&& clearFunc)
    : m_thread(&PoolWatcher::run, this)
    , m_clearFunc(clearFunc)
{
}

inline details::PoolWatcher::~PoolWatcher()
{
    stop();
}

inline void details::PoolWatcher::run()
{
    while (true) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_cv.wait(lock, [&]() {
                return m_stop || m_toClear;
            });

            if (m_stop) {
                return;
            }
        }
        m_cv.notify_all();

        if (m_toClear) {
            m_clearFunc(*m_toClear);
        }
        m_toClear = std::nullopt;
    }
}

inline void details::PoolWatcher::clear(std::thread::id id)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_toClear = id;
    }
    m_cv.notify_all();
}

inline void details::PoolWatcher::stop()
{
    if (!m_stop) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        m_thread.join();
    }
}

} // namespace fty
