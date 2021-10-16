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

class ITask;

class ThreadPool
{
public:
    enum class Stop
    {
        WaitForQueue,
        Immedialy,
        Cancel
    };

public:
    // Thread pool actions
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency() - 1);
    ThreadPool(size_t minNumThreads, size_t maxNumThreads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    size_t getCountAllocatedThreads() noexcept;

    void stop(Stop mode = Stop::WaitForQueue);
    void requestStop(Stop mode = Stop::WaitForQueue) noexcept;
    void waitUntilStopped();

public:
    // Managed Tasks in the pool
    template <typename T, typename... Args>
    std::shared_ptr<ITask> pushWorker(Args&&... args);

    template <typename Func, typename... Args>
    std::shared_ptr<ITask> pushWorker(Func&& fnc, Args&&... args);

    size_t getCountPendingTasks() noexcept;
    size_t getCountActiveTasks() noexcept;

private:
    void init();
    void taskRunner();
    void addTask(std::shared_ptr<ITask> task);

    void waitEndAllthreads() noexcept; // Used in waitUntilStopped and ~ThreadPool

private:
    const size_t m_minNumThreads;
    const size_t m_maxNumThreads;

    // List of threads in the pool
    std::vector<std::thread> m_threads;
    std::atomic<size_t>      m_countThreads = 0;
    std::mutex               m_tokenThreads;     // Manipulation on threads requires m_tokenThreads locked and !m_stopping
    std::atomic_bool         m_stopping = false; // This value can only be changed when m_tokenThreads is locked.
    std::atomic_bool         m_canceled = false; // This value can only be changed when m_tokenThreads is locked.


    // Management of tasks and task queue
    std::deque<std::shared_ptr<ITask>> m_tasks;
    std::atomic<size_t>                m_countPendingTasks = 0;
    std::mutex                         m_tokenTasks;
    std::condition_variable            m_cvTasks;
    std::atomic<size_t>                m_countActiveTasks = 0;
};

// ===========================================================================================================

class ITask
{
public:
    fty::Event<> started;
    fty::Event<> stopped;

    virtual ~ITask() = default;

    inline std::exception_ptr getException()
    {
        return m_eptr;
    }

public:
    virtual void operator()() = 0;

private:
    std::exception_ptr m_eptr = nullptr;
    friend class ThreadPool;
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

template <typename T>
Task<T>::Task() = default;

// ===========================================================================================================

inline ThreadPool::ThreadPool(size_t numThreads)
    : m_minNumThreads(numThreads)
    , m_maxNumThreads(numThreads)
{
    init();
}

inline ThreadPool::ThreadPool(size_t minNumThreads, size_t maxNumThreads)
    : m_minNumThreads(minNumThreads)
    , m_maxNumThreads(maxNumThreads)
{
    init();
}

inline void ThreadPool::init()
{
    if (m_maxNumThreads <= 0) {
        throw std::runtime_error("Impossible to create zero or less thread in the pool");
    }

    if (m_minNumThreads > m_maxNumThreads) {
        throw std::runtime_error("Minimum number of thread has to be smaller or equals to maximum");
    }

    // Create the threads pool (we need access to the thread list)
    std::unique_lock<std::mutex> lockThreads(m_tokenThreads);

    for (size_t i = 0; i < m_minNumThreads; ++i) {
        std::thread th(&ThreadPool::taskRunner, this);
        pthread_setname_np(th.native_handle(), "worker");
        m_threads.emplace_back(std::move(th));
        m_countThreads++;
    }
}

inline ThreadPool::~ThreadPool()
{
    // Do not start any new task, and wait to have the current one stopped
    requestStop(Stop::Immedialy);
    waitEndAllthreads();
}

inline size_t ThreadPool::getCountAllocatedThreads() noexcept
{
    return m_countThreads;
}

// add new task in the queue
template <typename T, typename... Args>
std::shared_ptr<ITask> ThreadPool::pushWorker(Args&&... args)
{
    // Create the task
    std::shared_ptr<T> task;
    task = std::make_shared<T>(std::forward<Args>(args)...);

    addTask(task);
    return task;
}

template <typename Func, typename... Args>
std::shared_ptr<ITask> ThreadPool::pushWorker(Func&& fnc, Args&&... args)
{
    // Create the task
    std::shared_ptr<details::GenericTask> task;
    task = std::make_shared<details::GenericTask>(std::move(fnc), std::forward<Args>(args)...);

    addTask(task);
    return task;
}

inline void ThreadPool::addTask(std::shared_ptr<ITask> task)
{
    if (m_stopping) {
        std::runtime_error("ThreadPool do not accept any tasks");
    }

    // Push it to the queue (we need to get the token to modify the queue)
    {
        std::unique_lock<std::mutex> lockTasks(m_tokenTasks);
        m_tasks.emplace_back(task);
        m_countPendingTasks++;
    }

    // Update the number of worker
    // If min and max are the same update are not needed
    if (m_minNumThreads != m_maxNumThreads) {

        // When do we need to add a worker:
        //   If the number of task being executed and in the queue are bigger than the current number of worker
        //   And number of worker is smaller than the maximum number of worker
        if (((m_countPendingTasks + m_countActiveTasks) > m_countThreads) && (m_countThreads < m_maxNumThreads)) {

            // Add a worker
            std::unique_lock<std::mutex> lockThreads(m_tokenThreads);
            if (m_stopping) {
                return;
            }

            std::thread th(&ThreadPool::taskRunner, this);
            pthread_setname_np(th.native_handle(), "worker");
            m_threads.emplace_back(std::move(th));
            m_countThreads++;
        }
    }

    // Notify one thread that new task is available
    m_cvTasks.notify_one();
}

inline size_t ThreadPool::getCountPendingTasks() noexcept
{
    // We need to count the threads
    return m_countPendingTasks;
}

inline size_t ThreadPool::getCountActiveTasks() noexcept
{
    return m_countActiveTasks;
}

inline void ThreadPool::stop(Stop mode)
{
    requestStop(mode);
    waitUntilStopped();
}

inline void ThreadPool::requestStop(Stop mode) noexcept
{
    {
        std::unique_lock<std::mutex> lockThread(m_tokenThreads);
        m_stopping = true;

        // When we want to do hard stop, we cancel all the thread
        if (mode == Stop::Cancel) {
            m_canceled = true;
            for (auto& th : m_threads) {
                pthread_cancel(th.native_handle());
            }
            m_countActiveTasks = 0;
        }
    }

    if (mode == Stop::Immedialy) {
        // We need to stop immediatly, so we empty the queue (we need to get the token to modify the queue)
        std::unique_lock<std::mutex> lockTasks(m_tokenTasks);
        m_countPendingTasks = m_countPendingTasks - m_tasks.size();
        m_tasks.clear();
    }

    m_cvTasks.notify_all();
}

inline void ThreadPool::waitUntilStopped()
{
    if (!m_stopping) {
        throw std::runtime_error("Stop hasn't been requested");
    }

    waitEndAllthreads();
}

inline void ThreadPool::waitEndAllthreads() noexcept
{
    // Wait that every thread pool finish => no need of mutex because we do not accept tasks
    for (auto threadIt = m_threads.begin(); threadIt != m_threads.end(); /*it++*/) {

        // Join the thread that can be join
        if (threadIt->joinable()) {
            threadIt->join();
        }
        threadIt = m_threads.erase(threadIt);
    }
}

inline void ThreadPool::taskRunner()
{
    // We stop the task runner with notifications
    while (true) {
        std::shared_ptr<ITask> task;

        // Try to find a task to achieve in the queue (we need to get the token to modify the queue)
        {
            std::unique_lock<std::mutex> lockTasks(m_tokenTasks);

            // We wait for something to do
            m_cvTasks.wait(lockTasks, [this]() {
                return !m_tasks.empty() || m_stopping;
            });

            if (m_stopping && m_tasks.empty()) {
                // We do not accept new task when we stop and if is nothing more to do. We terminate the task runner.
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop_front();
            m_countPendingTasks--;
        }

        // Execute the task
        if (task) {
            m_countActiveTasks++;
            task->started();

            try {
                (*task)();
            } catch (...) {
                if (m_canceled) {
                    // If we are canceled, the exception should reach the thread handler
                    throw;
                }
                // Transfer the exception to the task
                task->m_eptr = std::current_exception();
            }

            task->stopped();
            m_countActiveTasks--;
        }

        // Update the worker number if needed
        {
            // If min and max are the same update are not needed
            if (m_minNumThreads == m_maxNumThreads) {
                continue;
            }

            // Get the thread lock and check if we are still asked to run
            std::unique_lock<std::mutex> lockThread(m_tokenThreads);

            // We are asked to stop, no need to update worker
            if (m_stopping) {
                continue;
            }

            // When do we need to remove a worker?
            //   If the queue is empty => Nothing more for this worker
            //   And we didn't reach the minimum number of worker
            if ((m_countPendingTasks == 0) && (m_countThreads > m_minNumThreads)) {
                // I need to find myself in the list
                auto myId = std::this_thread::get_id();

                for (auto threadIt = m_threads.begin(); threadIt != m_threads.end(); threadIt++) {
                    if (threadIt->get_id() == myId) {

                        // Detach me before to remove me from the list (Avoid to create zombie)
                        threadIt->detach();
                        m_countThreads--;
                        m_threads.erase(threadIt);

                        // Terminate the thread
                        return;
                    }
                }
            }
        }
    }
}

} // namespace fty
