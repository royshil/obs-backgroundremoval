// SPDX-FileCopyrightText: Nicolai Josuttis
// SPDX-FileCopyrightText: Lewis Baker
// SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: CC-BY-4.0

// [MODIFIED]
// This file has been modified from the original version.
// Change: Namespace changed from 'std' to 'josuttis' to avoid conflicts with libc++.
// Date: 2025-12-22

// extended standard condition_variable to deal with
// interrupt tokens and jthread
// -----------------------------------------------------
#ifndef CONDITION_VARIABLE2_HPP
#define CONDITION_VARIABLE2_HPP

//*****************************************************************************
// forward declarations are in separate header due to cyclic type dependencies:
//*****************************************************************************
#include "stop_token.hpp"
#include <condition_variable>
#include <iostream>

namespace josuttis {


//*****************************************
//* class condition_variable_any2
//* - joining std::thread with interrupt support
//*****************************************
class condition_variable_any2
{
    template<typename Lockable>
    struct unlock_guard{
        unlock_guard(Lockable& mtx_):
            mtx(mtx_){
            mtx.unlock();
        }
        ~unlock_guard(){
            mtx.lock();
        }
        unlock_guard(unlock_guard const&)=delete;
        unlock_guard(unlock_guard&&)=delete;
        unlock_guard& operator=(unlock_guard const&)=delete;
        unlock_guard& operator=(unlock_guard&&)=delete;

    private:
        Lockable& mtx;
    };

    struct cv_internals{
        std::mutex m = {};
        std::condition_variable cv = {};

        void notify_all(){
            std::lock_guard<std::mutex> guard(m);
            cv.notify_all();
        }
        void notify_one(){
            std::lock_guard<std::mutex> guard(m);
            cv.notify_one();
        }
    };

  public:
    //*****************************************
    //* standardized API for condition_variable_any:
    //*****************************************

    condition_variable_any2()
        : internals{std::make_shared<cv_internals>()} {
    }
    ~condition_variable_any2() {
    }
    condition_variable_any2(const condition_variable_any2&) = delete;
    condition_variable_any2& operator=(const condition_variable_any2&) = delete;

    void notify_one() noexcept {
        internals->notify_one();
    }
    void notify_all() noexcept {
        internals->notify_all();
    }

    // wait()

    template<typename Lockable>
    void wait(Lockable& lock) {
        auto local_internals=internals;
        std::unique_lock<std::mutex> first_internal_lock(local_internals->m);
        unlock_guard<Lockable> unlocker(lock);
        std::unique_lock<std::mutex> second_internal_lock(std::move(first_internal_lock));
        local_internals->cv.wait(second_internal_lock);
    }

    template<class Lockable,class Predicate>
    void wait(Lockable& lock, Predicate pred) {
        // have to manually implement the loop so that the user-provided lock is reacquired before calling pred().
        // (otherwise the test_cvrace_pred test case fails)
        auto local_internals=internals;
        while (!pred()) {
          std::unique_lock<std::mutex> first_internal_lock(local_internals->m);
          unlock_guard<Lockable> unlocker(lock);
          std::unique_lock<std::mutex> second_internal_lock(std::move(first_internal_lock));
          local_internals->cv.wait(second_internal_lock);
        }
    }

    // wait_until()

    template<class Lockable, class Clock, class Duration>
     cv_status wait_until(Lockable& lock,
                          const chrono::time_point<Clock, Duration>& abs_time) {
        auto local_internals=internals;
        std::unique_lock<std::mutex> first_internal_lock(local_internals->m);
        unlock_guard<Lockable> unlocker(lock);
        std::unique_lock<std::mutex> second_internal_lock(std::move(first_internal_lock));
        return local_internals->cv.wait_until(second_internal_lock, abs_time);
    }

    template<class Lockable,class Clock, class Duration, class Predicate>
    bool wait_until(Lockable& lock,
                    const chrono::time_point<Clock, Duration>& abs_time,
                    Predicate pred) {
        // have to manually implement the loop so that the user-provided lock is reacquired before calling pred().
        // (otherwise the test_cvrace_pred test case fails)
        auto local_internals=internals;
        while (!pred()) {
            bool shouldStop;
            {
                std::unique_lock<std::mutex> first_internal_lock(local_internals->m);
                unlock_guard<Lockable> unlocker(lock);
                std::unique_lock<std::mutex> second_internal_lock(std::move(first_internal_lock));
                shouldStop = (local_internals->cv.wait_until(second_internal_lock, abs_time) == std::cv_status::timeout);
            }
            if (shouldStop) {
                return pred();
            }
        }
        return true;
    }

    // wait_for()

    template<class Lockable,class Rep, class Period>
    cv_status wait_for(Lockable& lock,
                       const chrono::duration<Rep, Period>& rel_time) {
        return wait_until(lock, std::chrono::steady_clock::now() + rel_time);
    }

    template<class Lockable,class Rep, class Period, class Predicate>
    bool wait_for(Lockable& lock,
                  const chrono::duration<Rep, Period>& rel_time,
                  Predicate pred) {
        return wait_until(lock, std::chrono::steady_clock::now() + rel_time, std::move(pred));
    }

    //*****************************************
    //* supplementary API:
    //*****************************************

    // x.6.2.1 dealing with interrupts:

    // return:
    // - true if pred() yields true
    // - false otherwise (i.e. on interrupt)
    template <class Lockable,class Predicate>
      bool wait(Lockable& lock,
                stop_token stoken,
                Predicate pred);

    // return:
    // - true if pred() yields true
    // - false otherwise (i.e. on timeout or interrupt)
    template <class Lockable, class Clock, class Duration, class Predicate>
      bool wait_until(Lockable& lock,
                      stop_token stoken,
                      const chrono::time_point<Clock, Duration>& abs_time,
                      Predicate pred);
    // return:
    // - true if pred() yields true
    // - false otherwise (i.e. on timeout or interrupt)
    template <class Lockable, class Rep, class Period, class Predicate>
      bool wait_for(Lockable& lock,
                    stop_token stoken,
                    const chrono::duration<Rep, Period>& rel_time,
                    Predicate pred);

  //*****************************************
  //* implementation:
  //*****************************************

  private:
    //*** API for the starting thread:
    std::shared_ptr<cv_internals> internals;
     // NOTE (as Howard Hinnant pointed out):
     // std::~condition_variable_any() says:
     //   Requires: There shall be no thread blocked on *this. [Note: That is, all threads shall have been notified;
     //             they may subsequently block on the lock specified in the wait.
     //             This relaxes the usual rules, which would have required all wait calls to happen before destruction.
     //             Only the notification to unblock the wait needs to happen before destruction.
     //             The user should take care to ensure that no threads wait on *this once the destructor has been started,
     //             especially when the waiting threads are calling the wait functions in a loop or using the overloads of
     //             wait, wait_for, or wait_until that take a predicate.  ]
     // That big long note means ~condition_variable_any() can execute before a signaled thread returns from a wait.
     // If this happens with condition_variable_any2, that waiting thread will attempt to lock the destructed mutex mut.
     // To fix this, there must be shared ownership of the data member mut between the condition_variable_any object
     // and the member functions wait (wait_for, etc.).
     // (libc++'s implementation gets this right: https://github.com/llvm-mirror/libcxx/blob/master/include/condition_variable
     //  It holds the data member mutex with a shared_ptr<mutex> instead of mutex directly, and the wait functions create
     //  a local shared_ptr<mutex> copy on entry so that if *this destructs out from under the thread executing the wait function,
     //  the mutex stays alive until the wait function returns.)
};



//*****************************************************************************
//* implementation of class condition_variable_any2
//*****************************************************************************

// wait_until(): wait with interrupt handling
// - returns on interrupt
// return value:
// - true if pred() yields true
// - false otherwise (i.e. on interrupt)
template <class Lockable, class Predicate>
inline bool condition_variable_any2::wait(Lockable& lock,
                                          stop_token stoken,
                                          Predicate pred)
{
    if (stoken.stop_requested()) {
      return pred();
    }
    auto local_internals=internals;
    stop_callback cb(stoken, [&local_internals] { local_internals->notify_all(); });
    while (!pred()) {
        std::unique_lock<std::mutex> first_internal_lock(local_internals->m);
        if (stoken.stop_requested()) {
            // pred() has already evaluated to 'false' since we last a acquired 'lock'
            return false;
        }
        unlock_guard<Lockable> unlocker(lock);
        std::unique_lock<std::mutex> second_internal_lock(std::move(first_internal_lock));
        local_internals->cv.wait(second_internal_lock);
    }

    return true;
}

// wait_until(): timed wait with interrupt handling
// - returns on interrupt
// return:
// - true if pred() yields true
// - false otherwise (i.e. on timeout or interrupt)
template <class Lockable, class Clock, class Duration, class Predicate>
inline bool condition_variable_any2::wait_until(Lockable& lock,
                                                stop_token stoken,
                                                const chrono::time_point<Clock, Duration>& abs_time,
                                                Predicate pred)
{
    if (stoken.stop_requested()) {
      return pred();
    }
    // have to manually implement the loop so that the user-provided lock is reacquired before calling pred().
    // (otherwise the test_cvrace_pred test case fails)
    auto local_internals=internals;
    stop_callback cb(stoken, [&local_internals] { local_internals->notify_all(); });
    while (!pred()) {
        bool shouldStop;
        {
            std::unique_lock<std::mutex> first_internal_lock(local_internals->m);
            if (stoken.stop_requested()) {
                // pred() has already evaluated to 'false' since we last acquired 'lock'.
                return false;
            }
            unlock_guard<Lockable> unlocker(lock);
            std::unique_lock<std::mutex> second_internal_lock(std::move(first_internal_lock));
            const auto status = local_internals->cv.wait_until(second_internal_lock, abs_time);
            shouldStop = (status == std::cv_status::timeout) || stoken.stop_requested();
        }
        if (shouldStop) {
            return pred();
        }
    }
    return true;
}

// wait_for(): timed wait with interrupt handling
// - returns on interrupt
// return:
// - true if pred() yields true
// - false otherwise (i.e. on timeout or interrupt)
template <class Lockable,class Rep, class Period, class Predicate>
inline bool condition_variable_any2::wait_for(Lockable& lock,
                                              stop_token stoken,
                                              const chrono::duration<Rep, Period>& rel_time,
                                              Predicate pred)
{
  auto abs_time = std::chrono::steady_clock::now() + rel_time;
  return wait_until(lock,
                    std::move(stoken),
                    abs_time,
                    std::move(pred));
}


} // josuttis

#endif // CONDITION_VARIABLE2_HPP
