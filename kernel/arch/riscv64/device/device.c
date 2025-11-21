/**
 * @file device.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备信息接口
 * @version alpha-1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <arch/riscv64/device/device.h>

#include <inttypes.h>

#include <basec/logger.h>
#include <sus/boot.h>
#include <libfdt.h>

static int errno;

/**
 * @brief 获取最近的设备错误码
 * 
 * @return int 错误码
 */
int device_get_errno(void) {
    return errno;
}

FDTDescriptor *device_check_initial(void *dtb_ptr) {
    FDTDescriptor *fdt = (FDTDescriptor *)dtb_ptr;

    /* 检查魔数 */
    if (fdt_magic(fdt) != FDT_MAGIC) {
        errno = -FDT_ERR_BADMAGIC;
        return nullptr;
    }

    /* 检查版本兼容性 */
    if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION) {
        errno = -FDT_ERR_BADVERSION;
        return nullptr;
    }

    /* 完整检查 */
    errno = fdt_check_header(fdt);
    if (errno != 0)
        return nullptr;
    return fdt;
}

// 设备树打印:打印缩进
static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        kprintf("  ");
    }
}

// 打印属性值（根据属性类型）
static void print_property_value(const char *name, const void *value, int len) {
    // 根据属性名称和长度判断类型并打印
    if (strcmp(name, "compatible") == 0 || 
        strcmp(name, "model") == 0 ||
        strcmp(name, "status") == 0 ||
        strcmp(name, "name") == 0 ||
        strcmp(name, "device_type") == 0) {
        // 字符串属性
        const char *str = value;
        int printed = 0;
        while (printed < len) {
            kprintf("\"%s\"", str);
            printed += strlen(str) + 1;
            str += strlen(str) + 1;
            if (printed < len) {
                kprintf(", ");
            }
        }
    } else if (strcmp(name, "reg") == 0) {
        // reg 属性 - 特殊处理
        kprintf("<寄存器>");
    } else if (strcmp(name, "interrupts") == 0) {
        // interrupts 属性 - 特殊处理
        kprintf("<中断>");
    } else if (strcmp(name, "phandle") == 0 && len == 4) {
        // phandle
        uint32_t phandle = fdt32_to_cpu(*(const uint32_t *)value);
        kprintf("%" PRIu32, phandle);
    } else if (len == 4) {
        // 32位整数
        uint32_t val = fdt32_to_cpu(*(const uint32_t *)value);
        kprintf("0x%" PRIx32 " (%" PRIu32 ")", val, val);
    } else if (len == 8) {
        // 64位整数
        uint64_t val = fdt64_to_cpu(*(const uint64_t *)value);
        kprintf("0x%" PRIx64 " (%" PRIu64 ")", val, val);
    } else if (len == 1) {
        // 字节
        uint8_t val = *(const uint8_t *)value;
        kprintf("0x%02x (%u)", val, val);
    } else {
        // 二进制数据
        kprintf("[");
        for (int i = 0; i < len && i < 16; i++) {
            kprintf("%02x", ((const unsigned char *)value)[i]);
            if (i < len - 1 && i < 15) {
                kprintf(" ");
            }
        }
        // 长度超过16字节时省略显示
        if (len > 16) {
            kprintf("...");
        }
        kprintf("] (%d bytes)", len);
    }
}

// 递归打印节点及其所有属性
static void print_node_recursive(const FDTDescriptor *fdt, int nd_off, int depth) {
    const char *node_name = fdt_get_name(fdt, nd_off, NULL);

    // 打印节点名称
    print_indent(depth);
    if (depth == 0)
        kprintf("/ {\n");
    else
        kprintf("%s {\n", node_name);

    int prop_off, child_off;

    // 遍历并打印所有属性
    fdt_for_each_property_offset(prop_off, fdt, nd_off) {
        // 获得属性
        const char *prop_name;
        const void *prop_value;
        int prop_len;

        prop_value = fdt_getprop_by_offset(fdt, prop_off, 
                                              &prop_name, &prop_len);

        // 打印属性名称和值
        if (prop_value) {
            print_indent(depth + 1);
            kprintf("%s = ", prop_name);
            print_property_value(prop_name, prop_value, prop_len);
            kprintf(";\n");
        }
        else {
            print_indent(depth + 1);
            kprintf("异常属性;\n");
        }
    }

    // 递归处理所有子节点
    fdt_for_each_subnode(child_off, fdt, nd_off) {
        print_node_recursive(fdt, child_off, depth + 1);
    }

    print_indent(depth);
    kprintf("}\n");
}

// 打印内存保留区域
static void print_memory_reservations(const FDTDescriptor *fdt) {
    kprintf("/* 内存保留区域 */\n");

    uint64_t address, size;
    int offset = 0;

    while (true) {
        // 若果没有更多保留区域则退出
        if (fdt_get_mem_rsv(fdt, offset, &address, &size) < 0) {
            break;
        }
        
        if (address == 0 && size == 0) {
            break;
        }
        
        // 打印保留区域的起始和结束地址
        kprintf(" 0x%" PRIx64 " 0x%" PRIx64, address, address + size - 1);
        offset++;
    }
}

// 统计函数
static void count_nodes_props(const FDTDescriptor *fdt, int nd_off, int *nodes, int *props) {
    (*nodes)++;
    
    // 遍历属性
    int prop_offset;
    fdt_for_each_property_offset(prop_offset, fdt, nd_off) {
        (*props)++;
    }
    
    // 递归遍历子节点
    int child_offset;
    fdt_for_each_subnode(child_offset, fdt, nd_off) {
        count_nodes_props(fdt, child_offset, nodes, props);
    }
}

// 主函数：打印整个设备树的所有信息
void print_entire_device_tree(const FDTDescriptor *fdt) {
    // 检查设备树有效性
    if (fdt_check_header(fdt) != 0) {
        log_error("无效的二进制设备树\n");
        return;
    }
    
    kprintf("设备树 \n");
    kprintf("总大小: %d 字节\n\n", fdt_totalsize(fdt));
    
    // 打印内存保留区域
    print_memory_reservations(fdt);
    
    // 打印根节点及其所有子节点
    print_node_recursive(fdt, 0, 0);
}

void print_device_tree_detailed(const FDTDescriptor *fdt) {
    if (fdt_check_header(fdt) != 0) {
        log_error("无效的二进制设备树\n");
        return;
    }
    
    kprintf("=== 设备树详细信息 ===\n\n");
    
    // 头部信息
    kprintf("  设备树文件头:\n");
    kprintf("  魔数: 0x%08x\n", fdt_magic(fdt));
    kprintf("  总大小: %d 字节\n", fdt_totalsize(fdt));
    kprintf("  结构块偏移: %d\n", fdt_off_dt_struct(fdt));
    kprintf("  字符串块偏移: %d\n", fdt_off_dt_strings(fdt));
    kprintf("  内存保留偏移: %d\n", fdt_off_mem_rsvmap(fdt));
    kprintf("  版本: %d\n", fdt_version(fdt));
    kprintf("  最后兼容版本: %d\n", fdt_last_comp_version(fdt));
    kprintf("  启动CPU ID: %d\n", fdt_boot_cpuid_phys(fdt));
    kprintf("  字符串块大小: %d\n", fdt_size_dt_strings(fdt));
    kprintf("  结构块大小: %d\n", fdt_size_dt_struct(fdt));
    kprintf("\n");
    
    // 内存保留区域详细信息
    kprintf("内存保留区域:\n");
    int i = 0;
    uint64_t address, size;
    while (fdt_get_mem_rsv(fdt, i, &address, &size) >= 0) {
        if (address == 0 && size == 0) break;
        kprintf("  保留区域 %d: 0x%016" PRIx64 " - 0x%016" PRIx64 " (大小: 0x%" PRIx64 " 字节)\n",
               i, address, address + size - 1, size);
        i++;
    }
    if (i == 0) {
        kprintf("  无内存保留区域\n");
    }
    kprintf("\n");
    
    // 节点统计
    kprintf("节点统计:\n");
    int total_nodes = 0;
    int total_properties = 0;
    
    count_nodes_props(fdt, 0, &total_nodes, &total_properties);
    kprintf("  总节点数: %d\n", total_nodes);
    kprintf("  总属性数: %d\n", total_properties);
    kprintf("\n");
    
    // 打印完整的设备树结构
    kprintf("完整的设备树结构:\n");
    kprintf("================================\n\n");
    print_entire_device_tree(fdt);
}