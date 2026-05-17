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

    /**
     * @brief 一次性调用回复对象的payload.
     *
     * ReplyPayload最多持有一条EndpointMessage, 用于endpoint_call的调用方
     * 阻塞等待endpoint_reply写入结果.
     */
    struct ReplyPayload : public _PayloadHelper<PayloadType::REPLY> {
        EndpointMessage *message = nullptr;
        WaitReasonId recv_wait_reason;

        ReplyPayload();
        ~ReplyPayload() override;
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

        /**
         * @brief 向endpoint写入消息.
         *
         * @param migrate_caps 可选数组. 对应项为true时, 该cap按迁移语义放入消息;
         * false或空指针时, 按普通CLONE语义复制cap.
         */
        Result<bool> send(pid_t sender_pid, const char *msgbuf, size_t msgsz,
                          Capability **caps, size_t capsz, bool blocking,
                          const bool *migrate_caps = nullptr);
        Result<EndpointMessage *> recv_async();
        Result<RecvAwaiter> recv_sync();
    };

    /**
     * @brief Reply Capability对象.
     *
     * 拥有REPLIER权限的一端可写入一次回复; 拥有CALLER权限的一端可阻塞读取.
     */
    class ReplyObject : public CapObj<ReplyPayload> {
    public:
        /**
         * @brief ReplyObject消息接收的协程等待器.
         */
        class RecvAwaiter {
        private:
            ReplyPayload *_payload = nullptr;

        public:
            explicit RecvAwaiter(ReplyPayload *payload) : _payload(payload) {}

            [[nodiscard]]
            bool await_ready() const {
                return false;
            }
            bool await_suspend(std::coroutine_handle<> handle);
            void await_resume() const {}
        };

        explicit ReplyObject(util::nonnull<Capability *> cap)
            : CapObj<ReplyPayload>(cap) {}

        /**
         * @brief 写入一次回复消息.
         *
         * 调用者必须持有REPLIER权限. ReplyObject已有消息时返回false.
         */
        Result<bool> send_reply(pid_t sender_pid, const char *msgbuf,
                                size_t msgsz, Capability **caps,
                                size_t capsz);
        /**
         * @brief 非阻塞读取ReplyObject中的回复消息.
         *
         * 调用者必须持有CALLER权限; 无消息时返回nullptr.
         */
        Result<EndpointMessage *> recv_async();
        /**
         * @brief 创建阻塞等待ReplyObject回复消息的awaiter.
         */
        Result<RecvAwaiter> recv_sync();
    };
}  // namespace cap
