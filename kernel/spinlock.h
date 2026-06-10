/**
 * @file spinlock.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 自旋锁
 * @version alpha-1.0.0
 * @date 2026-06-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <arch/riscv64/spinlock.h>
#include <task/scheduler.h>

class SpinLocker
{
private:
    raw_spinlock_t _lock = 0;
public:
    SpinLocker() = default;

    void lock()
    {
        // TODO: 将当前线程标记为不可抢断
        // 更准确地说, 添加一个方法, 记录不可抢断的次数
        // 次数归零时取消不可抢断
        __raw_spin_lock(&_lock);
    }

    void unlock()
    {
        // TODO: 将当前线程取消不可抢断的标记
        __raw_spin_unlock(&_lock);
    }
};

class GuardedLock
{
private:
    SpinLocker &_lock;
    bool locked = false;
    bool _preempt_was_disabled = false;
public:
    GuardedLock(SpinLocker &spinlock)
        : _lock(spinlock)
    {
        lock();
    }

    ~GuardedLock()
    {
        unlock();
    }

    void lock()
    {
        if (! locked)
        {
            if (! schd::Scheduler::initialized())
            {
                _preempt_was_disabled = true;
            }
            else
            {
                auto &scheduler = schd::Scheduler::inst();
                auto *current   = scheduler.current_tcb();
                if (current == nullptr)
                {
                    _preempt_was_disabled = true;
                }
                else
                {
                    _preempt_was_disabled = scheduler.preempt_disabled();
                    if (! _preempt_was_disabled)
                    {
                        auto preempt_res = scheduler.preempt_disable();
                        assert(preempt_res.has_value());
                    }
                }
            }
            _lock.lock();
            locked = true;
        }
    }

    void unlock()
    {
        if (locked)
        {
            _lock.unlock();
            if (! _preempt_was_disabled && schd::Scheduler::initialized())
            {
                auto preempt_res = schd::Scheduler::inst().preempt_enable();
                assert(preempt_res.has_value());
            }
            locked = false;
        }
    }
};
