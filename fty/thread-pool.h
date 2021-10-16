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
        std::optional<std::thread::id>       m_toClear;
        std::mutex                           m_mutex;
        std::atomic_bool                     m_stop = false;
        std::condition_variable              m_cv;
        std::function<void(std::thread::id)> m_clearFunc;
        std::thread                          m_thread;
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
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency() - 1);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void stop(Stop mode = Stop::WaitForQueue);

public:
    template <typename T, typename... Args>
    ITask& pushWorker(Args&&... args);

    template <typename Func, typename... Args>
    ITask& pushWorker(Func&& fnc, Args&&... args);

private:
    void allocThread();

private:
    size_t                             m_minNumThreads = 0;
    std::vector<std::thread>           m_threads;
    std::mutex                         m_mutex;
    std::condition_variable            m_cv;
    std::atomic_bool                   m_stop = false;
    std::deque<std::shared_ptr<ITask>> m_tasks;
    details::PoolWatcher               m_watcher;
};

// ===========================================================================================================


template <typename T>
Task<T>::Task() = default;

// ===========================================================================================================

inline ThreadPool::ThreadPool(size_t numThreads)
    : m_minNumThreads(numThreads)
    , m_watcher([&](std::thread::id id) {
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
    for (size_t i = 0; i < numThreads; ++i) {
        allocThread();
    }
}

inline ThreadPool::~ThreadPool()
{
    stop(Stop::Immedialy);
}

inline void ThreadPool::stop(Stop mode)
{
    if (!m_stop.load()) {
        m_watcher.stop();
        if (mode == Stop::WaitForQueue) {
            std::unique_lock<std::mutex> lock(m_mutex/*, std::defer_lock*/);
            m_cv.wait(lock, [&]() {
                return m_tasks.empty();
            });
        }

        m_stop.store(true);
        m_cv.notify_all();

        for (std::thread& thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        m_threads.clear();
    }
}

inline void ThreadPool::allocThread()
{
    using namespace std::chrono_literals;

    auto& th = m_threads.emplace_back(std::thread([&]() {
        while (!m_stop) {
            std::shared_ptr<ITask> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_cv.wait_for(lock, 1s, [&]() {
                    return !m_tasks.empty() || m_stop.load();
                });

                if (!m_stop.load() && m_tasks.empty() && m_threads.size() > m_minNumThreads) {
                    m_watcher.clear(std::this_thread::get_id());
                    return;
                }

                if (m_stop.load()) {
                    return;
                }

                if (!m_tasks.empty()) {
                    task = std::move(m_tasks.front());
                    m_tasks.pop_front();
                }
            }
            m_cv.notify_all();
            if (task) {
                task->started();
                (*task)();
                task->stopped();
            }
        }
    }));
    pthread_setname_np(th.native_handle(), "worker");
}

template <typename T, typename... Args>
ITask& ThreadPool::pushWorker(Args&&... args)
{
    std::shared_ptr<T> task;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_tasks.size() >= m_threads.size()) {
            allocThread();
        }
        task = std::make_shared<T>(std::forward<Args>(args)...);
        m_tasks.emplace_back(task);
    }
    m_cv.notify_all();
    return *task;
}

template <typename Func, typename... Args>
ITask& ThreadPool::pushWorker(Func&& fnc, Args&&... args)
{
    std::shared_ptr<details::GenericTask> task;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_tasks.size() >= m_threads.size()) {
            allocThread();
        }
        task = std::make_shared<details::GenericTask>(std::move(fnc), std::forward<Args>(args)...);
        m_tasks.emplace_back(task);
    }
    m_cv.notify_all();
    return *task;
}

// ===========================================================================================================

template <typename Func>
details::PoolWatcher::PoolWatcher(Func&& clearFunc)
    : m_clearFunc(clearFunc)
    , m_thread(&PoolWatcher::run, this)
{
    pthread_setname_np(m_thread.native_handle(), "pool watcher");
}

inline details::PoolWatcher::~PoolWatcher()
{
    stop();
}

inline void details::PoolWatcher::run()
{
    while (!m_stop.load()) {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_cv.wait(lock, [&]() {
            return m_stop.load() || m_toClear;
        });

        if (m_stop.load()) {
            return;
        }

        if (m_toClear) {
            m_clearFunc(*m_toClear);
            m_toClear = std::nullopt;
        }
    }
}

inline void details::PoolWatcher::clear(std::thread::id id)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_toClear = id;
    }
    m_cv.notify_one();
}

inline void details::PoolWatcher::stop()
{
    if (!m_stop.load()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop.store(true);
        }
        m_cv.notify_one();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
}

} // namespace fty
