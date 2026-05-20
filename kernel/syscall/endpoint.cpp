/**
 * @file endpoint.cpp
 * @brief Endpoint syscalls
 */

#include <cap/cholder.h>
#include <guard.h>
#include <logger.h>
#include <object/endpoint.h>
#include <object/perm.h>
#include <sus/coroutine.h>
#include <sus/nonnull.h>
#include <sus/raii.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>
#include <sustcore/msg.h>
#include <syscall/endpoint.h>
#include <syscall/syscall.h>
#include <syscall/uaccess.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <task/wait.h>

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstring>

namespace syscall {
    namespace {
        /**
         * @brief MsgPacket在内核中的地址视图.
         *
         * 用户态MsgPacket本身只在系统调用入口处复制一次; 接收路径会通过
         * packet_addr 将实际接收长度写回原MsgPacket.
         */
        struct KMsgPacket {
            /// 用户MsgPacket结构地址.
            VirAddr packet_addr;
            /// 用户消息缓冲区地址.
            VirAddr msgbuf;
            /// 发送时为消息大小, 接收时为用户消息缓冲容量.
            size_t msgsz = 0;
            /// 用户cap列表缓冲区地址.
            VirAddr caplist;
            /// 发送时为cap数量, 接收时为用户cap列表容量.
            size_t capsz = 0;
        };

        /**
         * @brief 已从用户态MsgPacket读取并校验过的待发送消息.
         */
        struct PreparedMsg {
            /// 内核中的消息正文副本.
            char msgbuf[MAX_MSG_SIZE]{};
            /// 消息正文大小.
            size_t msgsz = 0;
            /// 用户传入的cap索引副本.
            CapIdx capidxs[MAX_MSG_CAPS]{};
            /// 附带cap数量.
            size_t capsz = 0;
        };

        /**
         * @brief 查找并包装当前CSpace中的EndpointObject.
         */
        Result<cap::EndpointObject> endpoint_object(CapIdx capidx) {
            auto cap_res = cap::CHolder::lookup(capidx);
            propagate(cap_res);
            auto *cap = cap_res.value();
            if (cap->payload()->type_id() != PayloadType::ENDPOINT) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            return cap::EndpointObject(util::nnullforce(cap));
        }

        /**
         * @brief 查找并包装当前CSpace中的ReplyObject.
         */
        Result<cap::ReplyObject> reply_object(CapIdx capidx) {
            auto cap_res = cap::CHolder::lookup(capidx);
            propagate(cap_res);
            auto *cap = cap_res.value();
            if (cap->payload()->type_id() != PayloadType::REPLY) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            return cap::ReplyObject(util::nnullforce(cap));
        }

        /**
         * @brief 返回当前线程所属进程的pid.
         */
        pid_t current_pid() {
            auto *tcb = schd::Scheduler::inst().current_tcb();
            if (tcb == nullptr || tcb->task == nullptr) {
                return 0;
            }
            return tcb->task->pid;
        }

        /**
         * @brief 从用户态读取MsgPacket描述符.
         */
        Result<KMsgPacket> read_packet(VirAddr packet) {
            if (!packet.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }

            UBuffer packet_buf(packet, sizeof(MsgPacket));
            packet_buf.sync_from_user();
            auto *upacket = reinterpret_cast<MsgPacket *>(packet_buf.kbuf());

            KMsgPacket kpacket{
                .packet_addr = packet,
                .msgbuf      = packet + offsetof(MsgPacket, msgbuf),
                .msgsz       = upacket->msgsz,
                .caplist     = packet + offsetof(MsgPacket, caplist),
                .capsz       = upacket->capsz,
            };
            return kpacket;
        }

        /**
         * @brief 根据KMsgPacket读取消息正文和cap列表, 形成可发送消息.
         */
        Result<void> prepare_msg(const KMsgPacket &packet, PreparedMsg &out) {
            out.msgsz = packet.msgsz;
            out.capsz = packet.capsz;
            if (out.msgsz > MAX_MSG_SIZE || out.capsz > MAX_MSG_CAPS) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (out.msgsz != 0 && !packet.msgbuf.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (out.capsz != 0 && !packet.caplist.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }

            if (out.msgsz != 0) {
                UBuffer msg_buf(packet.msgbuf, out.msgsz);
                msg_buf.sync_from_user();
                memcpy(out.msgbuf, msg_buf.kbuf(), out.msgsz);
            }

            if (out.capsz != 0) {
                UBuffer caps_buf(packet.caplist, out.capsz * sizeof(CapIdx));
                caps_buf.sync_from_user();
                memcpy(out.capidxs, caps_buf.kbuf(),
                       out.capsz * sizeof(CapIdx));

            }

            void_return();
        }

        /**
         * @brief 将收到的消息附带cap插入目标CHolder空闲槽位.
         *
         * 若中途失败, 已插入的cap会被回滚移除.
         */
        Result<void> insert_received_caps(cap::CHolder *holder,
                                          cap::EndpointMessage *msg,
                                          CapIdx *out_caps) {
            if (holder == nullptr || msg == nullptr || out_caps == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            CapIdx inserted[MAX_MSG_CAPS]{};
            size_t inserted_count = 0;
            util::Guard cleanup([&]() {
                for (size_t i = 0; i < inserted_count; ++i) {
                    auto remove_res = holder->internal_remove(inserted[i]);
                    assert(remove_res.has_value());
                }
            });

            auto holder_id_res =
                task::TaskManager::inst().lookup_holder_id(msg->sender_pid);
            propagate(holder_id_res);

            auto sender_holder_res =
                cap::CHolderManager::inst().get_holder(holder_id_res.value());
            propagate(sender_holder_res);
            cap::CHolder *sender_holder = sender_holder_res.value();

            for (size_t i = 0; i < msg->capsz; ++i) {
                auto slot_res =
                    sender_holder->internal_transfer_to(*holder,
                                                        msg->capidxs[i]);
                propagate(slot_res);
                inserted[inserted_count++] = slot_res.value();
                out_caps[i]                = slot_res.value();
            }

            cleanup.release();
            void_return();
        }

        /**
         * @brief 将EndpointMessage写回用户态MsgPacket指定的缓冲区.
         */
        Result<void> write_received_msg(cap::CHolder *holder,
                                        cap::EndpointMessage *msg,
                                        const KMsgPacket &packet) {
            if (holder == nullptr || msg == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (msg->msgsz > packet.msgsz || msg->capsz > packet.capsz) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (msg->msgsz != 0 && !packet.msgbuf.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }

            CapIdx out_caps[MAX_MSG_CAPS]{};
            if (msg->capsz != 0) {
                if (!packet.caplist.nonnull()) {
                    unexpect_return(ErrCode::NULLPTR);
                }
                auto insert_res = insert_received_caps(holder, msg, out_caps);
                propagate(insert_res);
            }

            if (msg->msgsz != 0) {
                UBuffer out_msg(packet.msgbuf, msg->msgsz);
                memcpy(out_msg.kbuf(), msg->msgbuf, msg->msgsz);
                out_msg.sync_to_user();
            }

            if (msg->capsz != 0) {
                UBuffer out_caplist(packet.caplist,
                                    msg->capsz * sizeof(CapIdx));
                memcpy(out_caplist.kbuf(), out_caps,
                       msg->capsz * sizeof(CapIdx));
                out_caplist.sync_to_user();
            }

            UBuffer packet_buf(packet.packet_addr, sizeof(MsgPacket));
            packet_buf.sync_from_user();
            auto *upacket  = reinterpret_cast<MsgPacket *>(packet_buf.kbuf());
            upacket->msgsz = msg->msgsz;
            upacket->capsz = msg->capsz;
            packet_buf.sync_to_user();

            void_return();
        }

        /**
         * @brief 获取当前任务的CHolder.
         */
        Result<cap::CHolder *> current_holder() {
            return cap::CHolder::current();
        }

        cap::EndpointMsgView msg_view(PreparedMsg &msg) {
            return cap::EndpointMsgView{
                .msgbuf = msg.msgbuf,
                .msgsz  = msg.msgsz,
                .capidxs = msg.capidxs,
                .capsz  = msg.capsz,
            };
        }

        /**
         * @brief 读取并校验endpoint_call请求消息.
         */
        Result<PreparedMsg> prepare_call_msg(VirAddr sendmsg) {
            auto send_packet_res = read_packet(sendmsg);
            propagate(send_packet_res);

            PreparedMsg msg{};
            auto prep_res = prepare_msg(send_packet_res.value(), msg);
            propagate(prep_res);
            if (msg.capsz >= MAX_MSG_CAPS) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return msg;
        }

        /**
         * @brief 读取并校验endpoint_send消息.
         */
        Result<PreparedMsg> prepare_send_msg(VirAddr packet) {
            auto packet_res = read_packet(packet);
            propagate(packet_res);

            PreparedMsg msg{};
            auto prep_res = prepare_msg(packet_res.value(), msg);
            propagate(prep_res);
            return msg;
        }

        /**
         * @brief 读取并准备endpoint_reply回复消息.
         */
        Result<PreparedMsg> prepare_reply_msg(VirAddr replymsg) {
            auto packet_res = read_packet(replymsg);
            propagate(packet_res);

            PreparedMsg msg{};
            auto prep_res = prepare_msg(packet_res.value(), msg);
            propagate(prep_res);
            return msg;
        }

    }  // namespace

    Result<CapIdx> endpoint_create() {
        auto create_res = cap::CHolder::create<cap::EndpointPayload>();
        propagate(create_res);
        return create_res.value();
    }

    Result<bool> endpoint_send_async(CapIdx endpoint, VirAddr packet) {
        // prepare send message
        auto msg_res = prepare_send_msg(packet);
        propagate(msg_res);
        PreparedMsg msg = msg_res.value();

        auto endpoint_res = endpoint_object(endpoint);
        propagate(endpoint_res);
        cap::EndpointObject obj = endpoint_res.value();

        // to send asynchronously
        auto send_res = obj.send_async(current_pid(), msg_view(msg));
        propagate(send_res);
        return send_res.value();
    }

    Result<bool> endpoint_recv_async(CapIdx endpoint, VirAddr packet) {
        // prepare to receive
        auto packet_res = read_packet(packet);
        propagate(packet_res);

        auto holder_res = current_holder();
        propagate(holder_res);

        auto endpoint_res = endpoint_object(endpoint);
        propagate(endpoint_res);
        cap::EndpointObject obj = endpoint_res.value();

        // to receive asynchronously
        auto recv_res = obj.recv_async();
        propagate(recv_res);

        cap::EndpointMessage *msg = recv_res.value();
        if (msg == nullptr) {
            return false;
        }
        auto msg_guard = delete_guard(util::owner(msg));

        auto write_res =
            write_received_msg(holder_res.value(), msg, packet_res.value());
        propagate(write_res);

        return true;
    }

    util::cotask<Result<void>> endpoint_send_sync(CapIdx endpoint,
                                                  VirAddr packet) {
        // prepare send message
        auto msg_res = prepare_send_msg(packet);
        co_propagate(msg_res);
        PreparedMsg msg = msg_res.value();

        auto endpoint_res = endpoint_object(endpoint);
        co_propagate(endpoint_res);
        cap::EndpointObject obj = endpoint_res.value();

        // to send synchronously
        // so here we have to co_await the send method
        // until the message is actually sent
        auto send_res = co_await obj.send_sync(current_pid(), msg_view(msg));
        co_propagate(send_res);
        co_return Result<void>{};
    }

    util::cotask<Result<void>> endpoint_recv_sync(CapIdx endpoint,
                                                  VirAddr packet) {
        // prepare to receive
        auto packet_res = read_packet(packet);
        co_propagate(packet_res);

        auto holder_res = current_holder();
        co_propagate(holder_res);

        auto endpoint_res = endpoint_object(endpoint);
        co_propagate(endpoint_res);
        cap::EndpointObject endpoint_obj = endpoint_res.value();

        // to receive synchronously
        // so here we have to co_await the recv method
        // until a message is actually received
        auto recv_res = co_await endpoint_obj.recv_sync();
        co_propagate(recv_res);

        cap::EndpointMessage *msg = recv_res.value();
        auto msg_guard            = delete_guard(util::owner(msg));

        auto write_res =
            write_received_msg(holder_res.value(), msg, packet_res.value());
        co_propagate(write_res);
        co_return Result<void>{};
    }

    util::cotask<Result<void>> endpoint_call(CapIdx endpoint, VirAddr sendmsg,
                                             VirAddr replymsg) {
        // prepare the calling message
        auto msg_res = prepare_call_msg(sendmsg);
        co_propagate(msg_res);
        PreparedMsg msg = msg_res.value();

        auto holder_res = current_holder();
        co_propagate(holder_res);
        cap::CHolder *holder = holder_res.value();

        auto endpoint_res = endpoint_object(endpoint);
        co_propagate(endpoint_res);
        cap::EndpointObject endpoint_obj = endpoint_res.value();

        auto reply_packet_res = read_packet(replymsg);
        co_propagate(reply_packet_res);

        // do the call and co_await for the reply
        auto call_res = co_await endpoint_obj.call(
            current_pid(), holder, msg_view(msg));
        co_propagate(call_res);

        // write the reply message back to user space
        // here we use a guard to ensure the reply message is properly deleted
        // after we're done
        cap::EndpointMessage *reply = call_res.value();
        auto reply_guard            = delete_guard(util::owner(reply));

        // write into the user space
        auto write_reply_res =
            write_received_msg(holder, reply, reply_packet_res.value());
        co_propagate(write_reply_res);
        co_return Result<void>{};
    }

    Result<void> endpoint_reply(CapIdx reply_cap, VirAddr replymsg) {
        // prepare the reply message
        auto msg_res = prepare_reply_msg(replymsg);
        propagate(msg_res);
        PreparedMsg msg = msg_res.value();

        auto holder_res = current_holder();
        propagate(holder_res);

        auto reply_res = reply_object(reply_cap);
        propagate(reply_res);
        auto reply_obj = reply_res.value();

        return reply_obj.reply(current_pid(), holder_res.value(), reply_cap,
                               msg_view(msg));
    }
}  // namespace syscall
