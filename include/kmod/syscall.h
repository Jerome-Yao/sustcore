/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用接口
 * @version alpha-1.0.0
 * @date 2025-11-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <stddef.h>

/**
 * @brief 退出当前进程
 * 
 * @param code 退出码
 */
void exit(int code);

/**
 * @brief 注册中断处理程序
 * 
 * @param int_no 中断号
 * @param handler 中断处理函数
 */
void register_interrupt_handler(int int_no, void (*handler)(void));

/**
 * @brief 唤醒指定进程
 * 
 * @param pid 进程ID
 */
void wakeup_process(int pid);

/**
 * @brief 等待指定进程
 * 
 * @param pid 进程ID
 */
void wait_process(int pid);

/**
 * @brief 使当前进程休眠指定的毫秒数
 * 
 * @param ms 毫秒数
 */
void sleep(unsigned int ms);

/**
 * @brief 创建共享内存
 * 
 * @param size 共享内存大小
 * @return int 共享内存ID
 */
int makesharedmem(size_t size);

/**
 * @brief 共享内存给指定进程
 * 
 * @param pid 目标进程ID
 * @param shmid 共享内存ID
 */
void sharemem_with(int pid, int shmid);

/**
 * @brief 获取共享内存
 * 
 * @param id 共享内存ID
 * @return void* 指向共享内存的指针
 */
void *getsharedmem(int id);

/**
 * @brief 释放共享内存
 * 
 * @param id 共享内存ID
 */
void freesharedmem(int id);

/**
 * @brief 发送消息给指定进程
 * 
 * @param pid 进程ID
 * @param msg 消息指针
 * @param size 消息大小
 */
void send_message(int pid, const void *msg, size_t size);

#define RPC_CALL_MSG (0xFF)

/**
 * @brief 远程过程调用
 * 
 * @param pid        目标进程ID
 * @param fid        函数ID
 * @param args       参数指针
 * @param arg_size   参数大小
 * @param ret_buf    返回值缓冲区指针
 * @param ret_size   返回值缓冲区大小
 */
void rpc_call(int pid, int fid, const void *args, size_t arg_size, void *ret_buf, size_t ret_size);