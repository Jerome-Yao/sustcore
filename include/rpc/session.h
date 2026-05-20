/**
 * @file session.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RPC 会话类
 * @version alpha-1.0.0
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <rpc/packet.h>
#include <sustcore/capability.h>
#include <string_view>
#include <unordered_map>

namespace rpc {
    class Client;
    class Server;
    class ServerSession;
    class ClientSession;

    // 包装 Reply Object
    // 在服务端接收到来自 RPC 客户端的消息后
    // 会创建一个 Replier 对象来包装其附带的 Reply Object
    class Replier {
    protected:
        CapIdx _reply_cap;

    public:
        constexpr Replier(CapIdx reply_cap) : _reply_cap(reply_cap) {}
        Result<void> reply_session(const SessionResponsePacket &packet);
        Result<void> reply_close(const CloseResponsePacket &packet);
        Result<void> reply_response(const ResponsePacket &packet);
        Result<void> reply_error(const ErrorPacket &packet);
    };

    class Server {
        CapIdx _server_endpoint;
        std::string_view _server_name;
        sus_u32 _server_magic;
        std::unordered_map<sus_u32, util::owner<ServerSession *>> _sessions =
            {};
        sus_u32 _next_session_number = 1;

    public:
        constexpr Server(CapIdx server_endpoint, std::string_view server_name,
                         sus_u32 server_magic)
            : _server_endpoint(server_endpoint),
              _server_name(server_name),
              _server_magic(server_magic) {}
        virtual ~Server();

        virtual ServerSession *create_session(sus_u32 session_number) = 0;
        Result<ServerSession &> start();
        Result<void> on_session(const MsgPacket &msg, Replier &replier);

        /**
         * @brief 处理来自 RPC 客户端的消息, 该方法会被 ServerSession 调用,
         * 以实现对消息的分发和处理
         *
         * @param msg
         * @return Result<void>
         */
        Result<void> handle_message(const MsgPacket &msg);
    };

    class Client {
        CapIdx _server_endpoint;
        std::string_view _server_name;
        sus_u32 _server_magic;
        util::owner<ClientSession *> _session{nullptr};

    public:
        constexpr Client(CapIdx server_endpoint, std::string_view server_name,
                         sus_u32 server_magic)
            : _server_endpoint(server_endpoint),
              _server_name(server_name),
              _server_magic(server_magic) {}
        virtual ~Client();
        Result<ClientSession &> start();
        RPCErrorCode send_session(const SessionPacket &packet,
                                  sus_u32 &session_id);
        RPCErrorCode send_close(const ClosePacket &packet);
        RPCErrorCode send_call(const CallPacket &packet, MsgPacket &reply_msg);

        friend class ClientSession;
    };

    class ServerSession {
        Server &_server;
        sus_u32 _session_number;

    protected:
        constexpr ServerSession(Server &server, sus_u32 session_number)
            : _server(server), _session_number(session_number) {}

        Result<void> on_close(const MsgPacket &msg, Replier &replier);
        Result<void> on_call(const MsgPacket &msg, Replier &replier);

        struct CallResult {
            bool is_error;
            sus_u32 return_type;
            ByteBuffer retbuf;
        };

        virtual Result<void> handle_call(const CallPacket &msg, CallResult &result) = 0;

    public:
        virtual ~ServerSession() = default;

        friend class Server;
    };

    class ClientSession {
        Client &_client;
        size_t _session_number;

    protected:
        constexpr ClientSession(Client &client, sus_u32 session_number)
            : _client(client), _session_number(session_number) {}

    public:
        Result<ResponsePacket> call(sus_u32 function_id,
                                    const std::vector<sus_u32> &types,
                                    const ByteBuffer &argbuf,
                                    sus_u32 expected_return_type);
        Result<void> close();
        friend class Client;
    };
}  // namespace rpc
