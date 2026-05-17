/**
 * @file endpoint.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief IPC端点对象
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <object/perm.h>
#include <sus/list.h>
#include <sustcore/capability.h>
#include <task/task_struct.h>

#include <coroutine>
#include <cstddef>

namespace cap {
    struct EndpointMessage {
        pid_t sender_pid = 0;
        char msgbuf[MAX_MSG_SIZE]{};
        size_t msgsz = 0;
        Capability *caps[MAX_MSG_CAPS]{};
        size_t capsz = 0;
        util::ListHead<EndpointMessage> list_head{};

        EndpointMessage() = default;
        ~EndpointMessage();
    };

    struct EndpointPayload : public _PayloadHelper<PayloadType::ENDPOINT> {
        util::IntrusiveList<EndpointMessage, &EndpointMessage::list_head>
            messages = {};
        WaitReasonId send_wait_reason;
        WaitReasonId recv_wait_reason;

        EndpointPayload();
        ~EndpointPayload() override;
    };

    class EndpointObject : public CapObj<EndpointPayload> {
    public:
        /**
         * @brief Endpoint消息接收的协程等待器
         *
         */
        class RecvAwaiter {
        private:
            EndpointPayload *_payload = nullptr;

        public:
            explicit RecvAwaiter(EndpointPayload *payload)
                : _payload(payload) {}

            [[nodiscard]]
            bool await_ready() const {
                return false;
            }
            bool await_suspend(std::coroutine_handle<> handle);
            void await_resume() const {}
        };

        explicit EndpointObject(util::nonnull<Capability *> cap)
            : CapObj<EndpointPayload>(cap) {}

        Result<bool> send(pid_t sender_pid, const char *msgbuf, size_t msgsz,
                          Capability **caps, size_t capsz, bool blocking);
        Result<EndpointMessage *> recv_async();
        Result<RecvAwaiter> recv_sync();
    };
}  // namespace cap
