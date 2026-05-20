/**
 * @file session.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RPC 会话
 * @version alpha-1.0.0
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kmod/syscall.h>
#include <rpc/session.h>
#include <sustcore/msg.h>

#include <cassert>
#include <cstddef>

namespace rpc {
    namespace {
        Result<void> reply_message(CapIdx reply_cap, MsgPacket &msg) {
            endpoint_reply(reply_cap, &msg);
            void_return();
        }

        Result<MsgPacket> call_message(CapIdx endpoint, MsgPacket &request) {
            MsgPacket reply_packet;
            reply_packet.msgsz = MAX_MSG_SIZE;
            reply_packet.capsz = MAX_MSG_CAPS;

            endpoint_call(endpoint, &request, &reply_packet);
            return reply_packet;
        }

        RPCErrorCode packet_error_code(const MsgPacket &msg) {
            if (peek_type(msg) != PacketType::ERROR) {
                return RPCErrorCode::UNKNOWN_ERROR;
            }
            auto error_res = decode_error(msg);
            if (!error_res.has_value()) {
                return RPCErrorCode::UNKNOWN_ERROR;
            }
            return error_res.value().code;
        }

        Result<void> reply_unknown_error(Replier &replier, sus_u32 service_magic,
                                         sus_u32 session_id  = 0,
                                         sus_u32 function_id = 0) {
            ErrorPacket packet{
                .service_magic = service_magic,
                .session_id    = session_id,
                .function_id   = function_id,
                .code          = RPCErrorCode::UNKNOWN_ERROR,
            };
            return replier.reply_error(packet);
        }

        Result<void> reply_invalid_magic(Replier &replier, sus_u32 service_magic,
                                         sus_u32 session_id  = 0,
                                         sus_u32 function_id = 0) {
            ErrorPacket packet{
                .service_magic = service_magic,
                .session_id    = session_id,
                .function_id   = function_id,
                .code          = RPCErrorCode::INVALID_MAGIC,
            };
            return replier.reply_error(packet);
        }
    }  // namespace

    Result<void> Replier::reply_session(const SessionResponsePacket &packet) {
        auto msg_res = encode_session_response(packet);
        propagate(msg_res);
        auto msg = msg_res.value();
        return reply_message(_reply_cap, msg);
    }

    Result<void> Replier::reply_close(const CloseResponsePacket &packet) {
        auto msg_res = encode_close_response(packet);
        propagate(msg_res);
        auto msg = msg_res.value();
        return reply_message(_reply_cap, msg);
    }

    Result<void> Replier::reply_response(const ResponsePacket &packet) {
        auto msg_res = encode_response(packet);
        propagate(msg_res);
        auto msg = msg_res.value();
        return reply_message(_reply_cap, msg);
    }

    Result<void> Replier::reply_error(const ErrorPacket &packet) {
        auto msg_res = encode_error(packet);
        propagate(msg_res);
        auto msg = msg_res.value();
        return reply_message(_reply_cap, msg);
    }

    Server::~Server() {
        for (auto &[id, session] : _sessions) {
            delete session;
        }
        _sessions.clear();
    }

    Result<ServerSession &> Server::start() {
        sus_u32 session_number = _next_session_number;
        _next_session_number++;
        auto session = create_session(session_number);
        if (session == nullptr) {
            unexpect_return(ErrCode::ALLOCATION_FAILED);
        }
        _sessions[session_number] = util::owner<ServerSession *>(session);
        return std::ref(*session);
    }

    Result<void> Server::on_session(const MsgPacket &msg, Replier &replier) {
        auto packet_res = decode_session(msg);
        if (!packet_res.has_value()) {
            return reply_unknown_error(replier, _server_magic);
        }

        auto packet = packet_res.value();
        if (packet.service_magic != _server_magic) {
            return reply_invalid_magic(replier, _server_magic);
        }

        auto session_res = start();
        propagate(session_res);

        SessionResponsePacket response{
            .service_magic = _server_magic,
            .session_id    = session_res.value().get()._session_number,
        };
        return replier.reply_session(response);
    }

    Result<void> Server::handle_message(const MsgPacket &msg) {
        if (msg.capsz == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        Replier replier(msg.caplist[0]);
        auto packet_type = peek_type(msg);
        switch (packet_type) {
            case PacketType::SESSION: return on_session(msg, replier);
            case PacketType::CLOSE:   {
                auto packet_res = decode_close(msg);
                if (!packet_res.has_value()) {
                    return reply_unknown_error(replier, _server_magic);
                }

                auto packet = packet_res.value();
                if (packet.service_magic != _server_magic) {
                    return reply_invalid_magic(replier, _server_magic,
                                               packet.session_id);
                }

                auto iter = _sessions.find(packet.session_id);
                if (iter == _sessions.end()) {
                    return reply_unknown_error(replier, _server_magic,
                                               packet.session_id);
                }

                auto *session  = iter->second.get();
                auto close_res = session->on_close(msg, replier);
                propagate(close_res);
                delete session;
                _sessions.erase(iter);
                void_return();
            }
            case PacketType::CALL: {
                auto packet_res = decode_call(msg);
                if (!packet_res.has_value()) {
                    return reply_unknown_error(replier, _server_magic);
                }

                auto packet = packet_res.value();
                if (packet.service_magic != _server_magic) {
                    return reply_invalid_magic(replier, _server_magic,
                                               packet.session_id,
                                               packet.function_id);
                }

                auto iter = _sessions.find(packet.session_id);
                if (iter == _sessions.end()) {
                    return reply_unknown_error(replier, _server_magic,
                                               packet.session_id,
                                               packet.function_id);
                }

                return iter->second->on_call(msg, replier);
            }
            default:
                return reply_unknown_error(replier, _server_magic);
        }
    }

    Client::~Client() {
        if (_session != nullptr) {
            auto close_res = _session->close();
            assert(close_res.has_value());
            delete _session;
            _session = util::owner<ClientSession *>(nullptr);
        }
    }

    Result<ClientSession &> Client::start() {
        if (_session != nullptr) {
            if (_session->_session_number != 0) {
                return std::ref(*_session);
            }
            delete _session;
            _session = util::owner<ClientSession *>(nullptr);
        }

        sus_u32 session_id = 0;
        SessionPacket packet{
            .service_magic = _server_magic,
        };
        auto code = send_session(packet, session_id);
        if (code != RPCErrorCode::SUCCESS) {
            unexpect_return(ErrCode::FAILURE);
        }

        auto *session = new ClientSession(*this, session_id);
        if (session == nullptr) {
            unexpect_return(ErrCode::ALLOCATION_FAILED);
        }
        _session = util::owner<ClientSession *>(session);
        return std::ref(*session);
    }

    RPCErrorCode Client::send_session(const SessionPacket &packet,
                                      sus_u32 &session_id) {
        auto msg_res = encode_session(packet);
        if (!msg_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto msg       = msg_res.value();
        auto reply_res = call_message(_server_endpoint, msg);
        if (!reply_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto reply_msg   = reply_res.value();
        auto packet_type = peek_type(reply_msg);
        if (packet_type == PacketType::ERROR) {
            return packet_error_code(reply_msg);
        }
        if (packet_type != PacketType::SESSION_RESPONSE) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto response_res = decode_session_response(reply_msg);
        if (!response_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto response = response_res.value();
        if (response.service_magic != _server_magic) {
            return RPCErrorCode::INVALID_MAGIC;
        }
        session_id = response.session_id;
        return RPCErrorCode::SUCCESS;
    }

    RPCErrorCode Client::send_close(const ClosePacket &packet) {
        auto msg_res = encode_close(packet);
        if (!msg_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto msg       = msg_res.value();
        auto reply_res = call_message(_server_endpoint, msg);
        if (!reply_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto reply_msg   = reply_res.value();
        auto packet_type = peek_type(reply_msg);
        if (packet_type == PacketType::ERROR) {
            return packet_error_code(reply_msg);
        }
        if (packet_type != PacketType::CLOSE_RESPONSE) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto response_res = decode_close_response(reply_msg);
        if (!response_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto response = response_res.value();
        if (response.service_magic != _server_magic) {
            return RPCErrorCode::INVALID_MAGIC;
        }
        return RPCErrorCode::SUCCESS;
    }

    RPCErrorCode Client::send_call(const CallPacket &packet,
                                   MsgPacket &reply_msg) {
        auto msg_res = encode_call(packet);
        if (!msg_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        auto msg       = msg_res.value();
        auto reply_res = call_message(_server_endpoint, msg);
        if (!reply_res.has_value()) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }

        reply_msg        = reply_res.value();
        auto packet_type = peek_type(reply_msg);
        if (packet_type == PacketType::ERROR) {
            return packet_error_code(reply_msg);
        }
        if (packet_type != PacketType::RESPONSE) {
            return RPCErrorCode::UNKNOWN_ERROR;
        }
        return RPCErrorCode::SUCCESS;
    }

    Result<void> ServerSession::on_close(const MsgPacket &msg, Replier &replier) {
        auto packet_res = decode_close(msg);
        propagate(packet_res);

        auto packet = packet_res.value();
        if (packet.session_id != _session_number) {
            return reply_unknown_error(replier, packet.service_magic,
                                       packet.session_id);
        }

        CloseResponsePacket response{
            .service_magic = packet.service_magic,
            .session_id    = _session_number,
        };
        return replier.reply_close(response);
    }

    Result<void> ServerSession::on_call(const MsgPacket &msg,
                                        Replier &replier) {
        auto packet_res = decode_call(msg);
        propagate(packet_res);

        auto packet = packet_res.value();
        if (packet.session_id != _session_number) {
            return reply_unknown_error(replier, packet.service_magic,
                                       packet.session_id, packet.function_id);
        }

        CallResult result{
            .is_error    = false,
            .return_type = prim_typeid(PrimitiveTypeId::void_type),
            .retbuf      = ByteBuffer(0),
        };
        auto call_res = handle_call(packet, result);
        propagate(call_res);

        if (result.is_error) {
            ErrorPacket error{
                .service_magic = packet.service_magic,
                .session_id    = _session_number,
                .function_id   = packet.function_id,
                .code          = RPCErrorCode::UNKNOWN_ERROR,
            };
            return replier.reply_error(error);
        }

        ResponsePacket response{
            .service_magic = packet.service_magic,
            .session_id    = _session_number,
            .function_id   = packet.function_id,
            .return_type   = result.return_type,
            .retbuf        = result.retbuf,
        };
        return replier.reply_response(response);
    }

    Result<ResponsePacket> ClientSession::call(
        sus_u32 function_id, const std::vector<sus_u32> &types,
        const ByteBuffer &argbuf, sus_u32 expected_return_type) {
        if (_session_number == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        CallPacket packet{
            .service_magic = _client._server_magic,
            .session_id    = static_cast<sus_u32>(_session_number),
            .function_id   = function_id,
            .types         = types,
            .argbuf        = argbuf,
        };

        MsgPacket reply_msg{};
        auto code = _client.send_call(packet, reply_msg);
        if (code != RPCErrorCode::SUCCESS) {
            unexpect_return(ErrCode::FAILURE);
        }

        auto response_res = decode_response(reply_msg);
        propagate(response_res);
        auto response = response_res.value();
        if (response.service_magic != _client._server_magic ||
            response.session_id != _session_number ||
            response.function_id != function_id ||
            response.return_type != expected_return_type)
        {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        return response;
    }

    Result<void> ClientSession::close() {
        if (_session_number == 0) {
            void_return();
        }

        ClosePacket packet{
            .service_magic = _client._server_magic,
            .session_id    = static_cast<sus_u32>(_session_number),
        };
        auto code = _client.send_close(packet);
        if (code != RPCErrorCode::SUCCESS) {
            unexpect_return(ErrCode::FAILURE);
        }
        _session_number = 0;
        void_return();
    }
}  // namespace rpc
