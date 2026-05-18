/**
 * @file packet.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RPC 消息包
 * @version alpha-1.0.0
 * @date 2026-05-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <rpc/buffer.h>
#include <sus/types.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>
#include <sustcore/msg.h>

#include <cstddef>
#include <cstring>
#include <vector>

namespace rpc {
    template <typename T>
    struct dependent_false : std::false_type {};

    struct Message {
        bytebuffer data;
        std::vector<CapIdx> caps;

        Message(size_t capacity) : data(capacity) {}

        void build(MsgPacket &packet) {
            *packet.msgsz = data.size();
            packet.msgbuf = data.finish();
            if (!caps.empty()) {
                packet.caplist = caps.data();
                *packet.capsz  = caps.size();
            }
        }
    };

    enum class PacketType : b32 {
        CALL             = 1,
        RESPONSE         = 2,
        SESSION          = 3,
        SESSION_RESPONSE = 4,
        CLOSE            = 5,
        CLOSE_RESPONSE   = 6,
        ERROR            = 7,
    };

    enum class PrimitiveTypeId : b32 {
        u8      = 0,
        u16     = 1,
        u32     = 2,
        u64     = 3,
        i8      = 4,
        i16     = 5,
        i32     = 6,
        i64     = 7,
        f32     = 8,
        f64     = 9,
        size    = 10,
        off     = 11,
        boolean = 12,
    };

    template <typename T>
    struct ArrayView {
        const T *data{};
        b16 size{};
    };

    template <typename T>
    ArrayView<T> array_view(const T *data, b16 size) {
        return ArrayView<T>{data, size};
    }

    template <typename T>
    struct is_array_view : std::false_type {};

    template <typename T>
    struct is_array_view<ArrayView<T>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_array_view_v =
        is_array_view<std::remove_cvref_t<T>>::value;

}  // namespace rpc