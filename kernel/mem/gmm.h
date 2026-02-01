/**
 * @file gmm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通用内存管理器
 * @version alpha-1.0.0
 * @date 2026-01-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>

template <typename T>
concept GeneralMemoryManagerTrait = requires(int cnt, void *paddr) {
    {
        T::init()
    } -> std::same_as<void>;
    {
        T::get_page()
    } -> std::same_as<void *>;
    {
        T::get_page(cnt)
    } -> std::same_as<void *>;
    {
        T::put_page(paddr)
    } -> std::same_as<void>;
    {
        T::put_page(paddr, cnt)
    } -> std::same_as<void>;
    {
        T::clone_page(paddr)
    } -> std::same_as<void *>;
    {
        T::clone_page(paddr, cnt)
    } -> std::same_as<void *>;
};

class TrivalGMM {
public:
    static void init(void);
    static void *get_page(int cnt = 1);
    static void put_page(void *paddr, int cnt = 1);
    static void *clone_page(void *paddr, int cnt = 1);
};