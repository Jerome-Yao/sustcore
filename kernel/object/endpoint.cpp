/**
 * @file endpoint.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief IPC端点对象
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/int.h>
#include <env.h>
#include <logger.h>
#include <mem/vma.h>
#include <object/endpoint.h>
#include <object/perm.h>
#include <task/scheduler.h>
#include <task/wait.h>

#include <cassert>
#include <cstring>

namespace cap {
    namespace key {
        struct endpoint : public env::key::tmm {
        public:
            endpoint() = default;
        };
    }  // namespace key

    static void resume_recv(task::TCB *tcb) {
        if (tcb == nullptr || !tcb->coroutines.ipc_handle) {
            return;
        }

        auto *origin_tmm   = env::inst().tmm();
        PhyAddr origin_pgd = env::inst().pgd();
        auto *target_tmm   = tcb->task == nullptr ? nullptr : tcb->task->tmm;

        if (target_tmm != nullptr && target_tmm->pgd().nonnull() &&
            target_tmm->pgd() != origin_pgd)
        {
            env::inst().tmm(key::endpoint()) = target_tmm;
            PageMan(target_tmm->pgd()).switch_root();
            PageMan::flush_tlb();
        }

        tcb->coroutines.ipc_handle.resume();

        if (target_tmm != nullptr && target_tmm->pgd().nonnull() &&
            target_tmm->pgd() != origin_pgd)
        {
            env::inst().tmm(key::endpoint()) = origin_tmm;
            PageMan(origin_pgd).switch_root();
            PageMan::flush_tlb();
        }
    }

    EndpointMessage::~EndpointMessage() {
        for (size_t i = 0; i < capsz; ++i) {
            delete caps[i];
            caps[i] = nullptr;
        }
        capsz = 0;
    }

    EndpointPayload::EndpointPayload()
        : send_wait_reason(task::wait::alloc_reason()),
          recv_wait_reason(task::wait::alloc_reason()) {}

    EndpointPayload::~EndpointPayload() {
        while (!messages.empty()) {
            EndpointMessage *msg = &messages.front();
            messages.pop_front();
            delete msg;
        }
    }

    static Result<void> check_msg_bounds(size_t msgsz, size_t capsz) {
        if (msgsz > MAX_MSG_SIZE || capsz > MAX_MSG_CAPS) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        void_return();
    }

    Result<bool> EndpointObject::send(pid_t sender_pid, const char *msgbuf,
                                      size_t msgsz, Capability **caps,
                                      size_t capsz, bool blocking) {
        propagate(check_msg_bounds(msgsz, capsz));
        if (!imply(perm::endpoint::WRITE)) {
            loggers::CAPABILITY::ERROR("Endpoint WRITE权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (capsz != 0 && !imply(perm::endpoint::GRANT)) {
            loggers::CAPABILITY::ERROR("Endpoint GRANT权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        util::owner<EndpointMessage *> msg = util::owner(new EndpointMessage());
        auto msg_guard                     = util::Guard([&]() { delete msg; });

        if (msg == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        msg->sender_pid = sender_pid;
        msg->msgsz      = msgsz;
        msg->capsz      = capsz;
        if (msgsz != 0) {
            memcpy(msg->msgbuf, msgbuf, msgsz);
        }
        for (size_t i = 0; i < capsz; ++i) {
            if (caps[i] == nullptr || !caps[i]->imply(perm::basic::CLONE)) {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }
            msg->caps[i] = caps[i]->clone();
            if (msg->caps[i] == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
        }

        InterruptGuard guard;
        guard.enter();

        if (task::wait::has_waiting(_obj->recv_wait_reason)) {
            _obj->messages.push_back(*msg);
            msg_guard.release();
            auto recv_res = task::wait::peek_one(_obj->recv_wait_reason);
            if (recv_res.has_value() && recv_res.value() != nullptr) {
                resume_recv(recv_res.value());
            }
            auto wake_res = task::wait::wake_one(_obj->recv_wait_reason);
            return wake_res.transform(always(true));
        }

        // no waiting receiver, decide whether to block or not
        if (!blocking) {
            return false;
        }

        _obj->messages.push_back(*msg);
        auto wait_res = task::wait::wait_current(_obj->send_wait_reason);
        if (!wait_res.has_value()) {
            _obj->messages.remove(*msg);
            propagate_return(wait_res);
        }
        msg_guard.release();
        return true;
    }

    Result<EndpointMessage *> EndpointObject::recv_async() {
        if (!imply(perm::endpoint::READ)) {
            loggers::CAPABILITY::ERROR("Endpoint READ权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        InterruptGuard guard;
        guard.enter();

        if (_obj->messages.empty()) {
            return static_cast<EndpointMessage *>(nullptr);
        }

        EndpointMessage *msg = &_obj->messages.front();
        _obj->messages.pop_front();
        auto wake_res = task::wait::wake_one(_obj->send_wait_reason);
        propagate(wake_res);
        return msg;
    }

    bool EndpointObject::RecvAwaiter::await_suspend(
        std::coroutine_handle<> handle) {
        if (_payload == nullptr) {
            return false;
        }

        InterruptGuard guard;
        guard.enter();

        if (!_payload->messages.empty()) {
            return false;
        }

        loggers::CAPABILITY::INFO("Endpoint没有消息可接收, 线程进入等待");

        auto *receiver = schd::Scheduler::inst().current_tcb();
        if (receiver != nullptr) {
            receiver->coroutines.ipc_handle = handle;
        }
        auto wait_res = task::wait::wait_current(
            _payload->recv_wait_reason, [](task::TCB *tcb) {
                return tcb != nullptr && (!tcb->coroutines.syscall_pending ||
                                          tcb->coroutines.syscall_done);
            });
        if (!wait_res.has_value()) {
            if (receiver != nullptr) {
                receiver->coroutines.ipc_handle = nullptr;
            }
            loggers::CAPABILITY::ERROR("Endpoint接收等待失败: err=%d",
                                       wait_res.error());
            return false;
        }
        assert(receiver == nullptr ||
               receiver->coroutines.ipc_handle == handle);
        return true;
    }

    Result<EndpointObject::RecvAwaiter> EndpointObject::recv_sync() {
        if (!imply(perm::endpoint::READ)) {
            loggers::CAPABILITY::ERROR("Endpoint READ权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        return RecvAwaiter(_obj);
    }
}  // namespace cap
