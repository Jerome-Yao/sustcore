/**
 * @file capdef.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力系统声明与常量定义
 * @version alpha-1.0.0
 * @date 2026-02-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/optional.h>
#include <sustcore/cap_type.h>

#include <concepts>

template <typename T>
using CapOptional = util::Optional<T, CapErrCode, CapErrCode::SUCCESS,
                                   CapErrCode::UNKNOWN_ERROR>;

constexpr size_t CSPACE_MAX_SLOTS = 1024;

template <typename Payload>
concept PayloadTrait = requires(Payload *p) {
    {
        p->retain()
    } -> std::same_as<void>;
    {
        p->release()
    } -> std::same_as<void>;
    {
        p->ref_count()
    } -> std::same_as<int>;
    {
        Payload::PAYLOAD_IDENTIFIER
    } -> std::convertible_to<CapType>;
    {
        Payload::SPACE_SIZE
    } -> std::convertible_to<size_t>;
    {
        Payload::SPACE_COUNT
    } -> std::convertible_to<size_t>;
    typename Payload::CCALL;
} && (Payload::SPACE_SIZE <= CSPACE_MAX_SLOTS);

template <PayloadTrait Payload>
class Capability;

// CSpaces
class CSpaceBase;
template <PayloadTrait Payload>
class CSpace;
template <PayloadTrait Payload>
class CUniverse;
template <typename... Payloads>
class _CapHolder;

// 新的需要管理的内核对象应该追加到此处
using CapHolder = _CapHolder<CSpaceBase /*, other kernel objects*/>;