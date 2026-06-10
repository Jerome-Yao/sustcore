/**
 * @file block.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 块设备接口
 * @version alpha-1.0.0
 * @date 2026-02-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bio/request.h>
#include <driver/base.h>
#include <sus/rtti.h>
#include <sustcore/errcode.h>

#include <cstddef>

enum class BlockDeviceType { BASIC = 0, RAMDISK = 1 };

/**
 * @brief 块设备统一抽象接口.
 *
 * 该接口描述 Sustcore 中块设备对上层暴露的最小能力集合:
 *
 * 1. 容量与块大小查询.
 * 2. 绑定设备专属请求队列.
 * 3. 消费已进入 PROCESSING 状态的块请求.
 * 4. 运行设备 worker 主循环.
 *
 * 当前块层的执行模型不是同步 `read_blocks()/write_blocks()` 调用,
 * 而是 `BufferCache -> BlockRequestLayer -> BlockRequestQueue -> device`.
 * 设备驱动在 `process_request()` 中接管请求后, 必须负责推进该请求到完成态.
 */
class IBlockDeviceOps : public RTTIBase<IBlockDeviceOps, BlockDeviceType> {
public:
    virtual ~IBlockDeviceOps() = default;
    /**
     * @brief 获得块大小
     *
     * @return size_t 块大小 (字节）
     */
    [[nodiscard]]
    virtual Result<size_t> block_sz(void) const = 0;
    /**
     * @brief 获得块数量
     *
     * @return size_t 块数量
     */
    [[nodiscard]]
    virtual Result<size_t> block_cnt(void) const = 0;
    /**
     * @brief 设备是否只读
     *
     * @return bool 是否只读
     */
    [[nodiscard]]
    virtual Result<bool> readonly(void) const {
        return false;
    }
    /**
     * @brief 获得设备总字节数
     *
     * @return size_t 设备总字节数
     */
    [[nodiscard]]
    virtual Result<size_t> total_bytes(void) const {
        auto block_sz_res = block_sz();
        propagate(block_sz_res);
        auto block_cnt_res = block_cnt();
        propagate(block_cnt_res);
        return block_sz_res.value() * block_cnt_res.value();
    }
    /**
     * @brief 绑定该设备唯一对应的请求队列.
     *
     * 该接口只在设备注册阶段调用一次. 绑定完成后, 设备可在
     * `process_request()` 与 `run_request_loop()` 中使用该队列推进请求状态机.
     *
     * @param queue 设备专属请求队列, 必须非空.
     * @return Result<void> 绑定结果.
     */
    [[nodiscard]]
    virtual Result<void> bind_request_queue(
        util::nonnull<blk::BlockRequestQueue *> queue) = 0;
    /**
     * @brief 处理一个已进入 PROCESSING 状态的请求.
     *
     * 调用方保证传入请求已经由 worker 从队列中取出并标记为
     * `BlockReqStatus::PROCESSING`.
     *
     * 驱动必须在该函数执行路径中负责完成请求, 即通过对应请求队列的
     * `complete()` 将请求推进到 `COMPLETED/FAILED`.
     *
     * @param req 待处理的块请求.
     * @return Result<void> 处理流程是否成功推进.
     */
    [[nodiscard]]
    virtual Result<void> process_request(blk::BlockRequest &req) = 0;
    /**
     * @brief 运行设备专属 worker 主循环.
     *
     * 典型实现会循环执行:
     * `wait_and_dequeue() -> mark_processing() -> process_request()`.
     *
     * @return Result<void> 主循环退出结果.
     */
    [[nodiscard]]
    virtual Result<void> run_request_loop() = 0;
};

namespace driver {
    /**
     * @brief 硬件块设备驱动基类.
     *
     * 真实块设备驱动通常同时需要设备框架的 `DriverBase` 能力与块层的
     * `IBlockDeviceOps` 接口, 因此优先继承该基类.
     */
    class BlockDevice : public DriverBase, public IBlockDeviceOps {
    public:
        explicit BlockDevice(DevRes res) noexcept
            : DriverBase(std::move(res)) {}
        ~BlockDevice() override = default;
    };
}  // namespace driver

/**
 * @brief 内存后端块设备实现.
 *
 * RamDisk 使用一段连续内存作为底层存储介质. 它内部仍保留同步
 * `read_blocks()/write_blocks()/sync()` 原语, 但对块层暴露的执行模型
 * 与其他设备一致: 请求由 worker 取出后交给 `process_request()`,
 * 再由驱动显式完成请求.
 *
 * `base()` 仅供少数上层快路径(如 TarFS 的 RamDisk 挂载优化)使用,
 * 不是通用块抽象的一部分.
 *
 * 当前 `RamDiskDevice` 的生命周期由调用方外部保证, `BlkManager`
 * 只负责块层配套资源, 不负责销毁该设备对象.
 *
 * TODO: 后续需要统一内存后端设备与硬件设备的生命周期管理策略.
 */
class RamDiskDevice : public IBlockDeviceOps {
private:
    void *D_base;
    size_t D_block_size;
    size_t D_block_count;
    blk::BlockRequestQueue *_queue = nullptr;

public:
    constexpr static BlockDeviceType IDENTIFIER = BlockDeviceType::RAMDISK;
    [[nodiscard]]
    BlockDeviceType type_id() const override
    {
        return IDENTIFIER;
    }
    ~RamDiskDevice() override = default;
    /**
     * @brief 构造一个内存后端块设备.
     *
     * 调用方必须保证底层内存区域与设备对象本身的生命周期至少覆盖
     * 其在 `BlkManager` 中注册存活的整个阶段.
     */
    constexpr RamDiskDevice(void *base, size_t block_size, size_t block_count)
        : D_base(base), D_block_size(block_size), D_block_count(block_count) {}
    [[nodiscard]]
    Result<size_t> block_sz() const override
    {
        return D_block_size;
    }
    [[nodiscard]]
    Result<size_t> block_cnt() const override
    {
        return D_block_count;
    }
    /**
     * @brief 记录绑定到该设备的专属请求队列.
     */
    [[nodiscard]]
    Result<void> bind_request_queue(
        util::nonnull<blk::BlockRequestQueue *> queue) override;
    /**
     * @brief 按请求类型分派到 RamDisk 的同步原语并完成请求.
     */
    [[nodiscard]]
    Result<void> process_request(blk::BlockRequest &req) override;
    /**
     * @brief 运行 RamDisk 的请求消费主循环.
     */
    [[nodiscard]]
    Result<void> run_request_loop() override;
    [[nodiscard]]
    constexpr void *base() const {
        return D_base;
    }

    /**
     * @brief 从内存后端同步读取若干块.
     *
     * 该接口是 RamDisk 的内部实现原语, 不属于通用块设备抽象的对外契约.
     */
    [[nodiscard]]
    Result<size_t> read_blocks(lba_t lba, void *buf, size_t cnt);
    /**
     * @brief 向内存后端同步写入若干块.
     *
     * 该接口是 RamDisk 的内部实现原语, 不属于通用块设备抽象的对外契约.
     */
    [[nodiscard]]
    Result<size_t> write_blocks(lba_t lba, const void *buf, size_t cnt);
    /**
     * @brief 同步 RamDisk 状态.
     *
     * 对 RamDisk 而言该操作当前为空操作.
     */
    [[nodiscard]]
    Result<void> sync(void);
};
