/**
 * @file virtio-blk.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief virtio 块设备驱动
 * @version alpha-1.0.0
 * @date 2026-06-07
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <driver/virtio/virtio.h>

namespace virtio {
    namespace feature {
        constexpr le32 MASK_SIZE_MAX     = 1 << 1;
        constexpr le32 MASK_SEG_MAX      = 1 << 2;
        constexpr le32 MASK_GEOMETRY     = 1 << 4;
        constexpr le32 MASK_RO           = 1 << 5;
        constexpr le32 MASK_BLK_SIZE     = 1 << 6;
        constexpr le32 MASK_FLUSH        = 1 << 9;
        constexpr le32 MASK_TOPOLOGY     = 1 << 10;
        constexpr le32 MASK_CONFIG_WCE   = 1 << 11;
        constexpr le32 MASK_MQ           = 1 << 12;
        constexpr le32 MASK_DISCARD      = 1 << 13;
        constexpr le32 MASK_WRITE_ZEROES = 1 << 14;
        constexpr le32 MASK_LIFETIME     = 1 << 15;
        constexpr le32 MASK_SECURE_ERASE = 1 << 16;
    }  // namespace feature
    struct BlkConfig {
        le64 capacity;
        le32 size_max;
        le32 seg_max;
        struct BlkGeometry {
            le16 cylinders;
            u8 heads;
            u8 sectors;
        } geometry;
        le32 blk_size;
        struct BlkTopology {
            // # of logical blocks per physical block (log2)
            u8 physical_block_exp;
            // offset of first aligned logical block
            u8 alignment_offset;
            // suggested minimum I/O size in blocks
            le16 min_io_size;
            // optimal (suggested maximum) I/O size in blocks
            le32 opt_io_size;
        } topology;
        u8 writeback;
        u8 unused0;
        u16 num_queues;
        le32 max_discard_sectors;
        le32 max_discard_seg;
        le32 discard_sector_alignment;
        le32 max_write_zeroes_sectors;
        le32 max_write_zeroes_seg;
        u8 write_zeroes_may_unmap;
        u8 unused1[3];
        le32 max_secure_erase_sectors;
        le32 max_secure_erase_seg;
        le32 secure_erase_sector_alignment;
    };

    struct BlkReq {
        le32 type;
        le32 reserved;
        le64 sector;
    };

    enum class BlkReqType {
        IN           = 0,
        OUT          = 1,
        FLUSH        = 4,
        DISCARD      = 11,
        WRITE_ZEROES = 13
    };

    enum class BlkReqStatus { OK = 0, IOERR = 1, UNSUPP = 2 };
}  // namespace virtio