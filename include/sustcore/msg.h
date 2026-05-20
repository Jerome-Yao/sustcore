/**
 * @file msg.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 消息包
 * @version alpha-1.0.0
 * @date 2026-05-18
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>

constexpr size_t MAX_MSG_SIZE = 128;
constexpr size_t MAX_MSG_CAPS = 4;

/**
 * @brief Endpoint IPC消息描述符.
 *
 * 发送时, msgsz/capsz 是输入长度; 接收时, msgsz/capsz 是用户缓冲容量,
 * 内核返回前会写回实际收到的字节数与cap数量.
 * msgbuf 和 caplist 是固定容量内嵌缓冲区.
 */
struct MsgPacket {
    // 消息数据缓冲区.
    byte msgbuf[MAX_MSG_SIZE];
    // 消息字节数或接收缓冲容量.
    size_t msgsz = 0;
    // Capability索引列表缓冲区.
    CapIdx caplist[MAX_MSG_CAPS];
    // Capability数量或接收cap列表容量.
    size_t capsz = 0;
};
