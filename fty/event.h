#pragma once
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

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
