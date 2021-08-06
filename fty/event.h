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
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include <fty/expected.h>

namespace fty {

// ===========================================================================================================

template <typename...>
class Event;

// ===========================================================================================================

template <typename... Args>
class Slot
{
public:
    class Impl
    {
    public:
        template <typename Func, typename Cls>
        Impl(Func&& func, Cls* cls);
        template <typename Func>
        Impl(Func&& func);

        void call(Args&&... args) const;

    private:
        std::function<void(Args...)> m_function;
        mutable std::mutex           m_mutex;
    };

public:
    template <typename Func, typename Cls>
    Slot(Func&& func, Cls* cls);
    template <typename Func>
    Slot(Func&& func);
    Slot(Slot&&) = default;
    Slot(const Slot&) = default;
    ~Slot();

    void connect(Event<Args...>& signal);

private:
    friend class Event<Args...>;
    std::shared_ptr<Impl> m_impl;
};

// ===========================================================================================================

template <typename... Args>
class Event
{
public:
    ~Event();

    Event() = default;

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    Event(Event&& other)
        : m_connections(std::move(other.m_connections))
    {
    }

    Event& operator=(Event&& other)
    {
        std::swap(m_connections, other.m_connections);
    }

    void operator()(Args&&... args) const;

    void connect(Slot<Args...>& slot);

    void wait();

    Expected<void> wait(int msecTimeout);

    template <typename Rep, typename Period>
    Expected<void> wait(const std::chrono::duration<Rep, Period>& timeout);

private:
    using Connections = std::vector<std::weak_ptr<typename Slot<Args...>::Impl>>;

    mutable Connections             m_connections;
    mutable std::mutex              m_mutex;
    mutable std::condition_variable m_cv;
    bool                            m_stopped = false;
    mutable bool                    m_fired   = false;
};

// ===========================================================================================================

template <typename... Args>
Event<Args...>::~Event()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopped = true;
    }
    m_cv.notify_all();
}

template <typename... Args>
void Event<Args...>::operator()(Args&&... args) const
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Connections                 toRemove;

        for (auto iter = m_connections.begin(); iter != m_connections.end();) {
            if (auto caller = iter->lock()) {
                caller->call(std::forward<Args>(args)...);
                ++iter;
            } else {
                m_connections.erase(iter);
            }
        }
        m_fired = true;
    }
    m_cv.notify_all();
}

template <typename... Args>
void Event<Args...>::connect(Slot<Args...>& slot)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.emplace_back(slot.m_impl);
}

template <typename... Args>
void Event<Args...>::wait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [&]() {
        return m_stopped || m_fired;
    });
    m_fired = false;
}

template <typename... Args>
Expected<void> Event<Args...>::wait(int mSecTimeout)
{
    return wait(std::chrono::milliseconds(mSecTimeout));
}

template <typename... Args>
template <typename Rep, typename Period>
Expected<void> Event<Args...>::wait(const std::chrono::duration<Rep, Period>& timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto ret = m_cv.wait_for(lock, timeout, [&]() {
        return m_stopped || m_fired;
    });

    if (!ret) {
        return unexpected("timeout");
    } else {
        m_fired = false;
        return {};
    }
}

// ===========================================================================================================

template <typename... Args>
template <typename Func, typename Cls>
Slot<Args...>::Impl::Impl(Func&& func, Cls* cls)
    : m_function([fnc = std::move(func), caller = std::move(cls)](Args&&... args) {
        std::invoke(fnc, caller, std::forward<Args>(args)...);
    })
{
}

template <typename... Args>
template <typename Func>
Slot<Args...>::Impl::Impl(Func&& func)
    : m_function(std::move(func))
{
}

template <typename... Args>
void Slot<Args...>::Impl::call(Args&&... args) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_function(std::forward<Args>(args)...);
}

// ===========================================================================================================

template <typename... Args>
template <typename Func, typename Cls>
Slot<Args...>::Slot(Func&& func, Cls* cls)
    : m_impl(new Impl(std::forward<Func>(func), cls))
{
}

template <typename... Args>
template <typename Func>
Slot<Args...>::Slot(Func&& func)
    : m_impl(new Impl(std::forward<Func>(func)))
{
}

template <typename... Args>
Slot<Args...>::~Slot()
{
}

template <typename... Args>
void Slot<Args...>::connect(Event<Args...>& signal)
{
    signal.connect(*this);
}

// ===========================================================================================================

} // namespace fty
