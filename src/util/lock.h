#pragma once

#include <atomic>

#include <atomic>
#include <thread>

class spinlock {
public:
    spinlock() noexcept = default;

    spinlock(const spinlock &) = delete;

    spinlock &operator=(const spinlock &) = delete;

    spinlock(spinlock &&) noexcept {
         flag_.clear(std::memory_order_relaxed);
    }

    spinlock &operator=(spinlock &&) noexcept {
         flag_.clear(std::memory_order_relaxed);
        return *this;
    }

    void lock() noexcept {
         while (flag_.test_and_set(std::memory_order_acquire)) {}
    }

    bool try_lock() noexcept {
         return !flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};


class unique_spinlock {
public:
    unique_spinlock() = default;

    explicit unique_spinlock(spinlock &lck)
            : lck_(&lck)
            {
        lck.lock();
    }

    ~unique_spinlock() {
        if (lck_)
            lck_->unlock();
    }

    static unique_spinlock try_acquire(spinlock &lck) {
        unique_spinlock rval;
        if (lck.try_lock())
            rval.lck_ = &lck;

        return rval;
    }

    unique_spinlock(const unique_spinlock &) = delete;

    unique_spinlock &operator=(const unique_spinlock &) = delete;

    unique_spinlock(unique_spinlock &&other) noexcept {
        std::swap(lck_, other.lck_);
    }

    unique_spinlock &operator=(unique_spinlock &&other) noexcept {
        std::swap(lck_, other.lck_);
        return *this;
    }

    void unlock() noexcept {
        if (lck_) {
            lck_->unlock();
            lck_ = nullptr;
        }
    }

    [[nodiscard]] bool owns_lock() const noexcept {
        return lck_ != nullptr;
    }

private:
    spinlock *lck_ = nullptr;
};
