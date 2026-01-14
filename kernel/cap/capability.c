/**
 * @file capability.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability系统
 * @version alpha-1.0.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <assert.h>
#include <cap/capability.h>
#include <mem/alloc.h>
#include <string.h>
#include <sus/list_helper.h>
#include <task/task_struct.h>

#ifdef DLOG_CAP
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

/**
 * @brief 创建一个新的CSpace
 *
 * @return CSpace* 新的CSpace指针
 */
CSpace new_cspace(void) {
    CSpace space = (CSpace)kmalloc(sizeof(Capability *) * CSPACE_ITEMS);
    memset(space, 0, sizeof(Capability *) * CSPACE_ITEMS);
    return space;
}

Capability *fetch_cap(PCB *pcb, CapIdx idx) {
    log_info("fetch_cap: cspace=%d, cindex=%d", idx.cspace, idx.cindex);

    // PCB中无CSpaces
    if (pcb->cap_spaces == nullptr) {
        log_error("fetch_cap: PCB块中无CSpaces");
        return nullptr;
    }

    // 指定的CapIdx无效
    if (CAPIDX_INVALID(idx)) {
        log_error("fetch_cap: 指定的CapIdx无效");
        return nullptr;
    }

    // 对应CSpace不存在
    if (idx.cspace < 0 || idx.cspace >= PROC_CSPACES) {
        log_error("fetch_cap: CSpace超出范围");
        return nullptr;
    }

    if (pcb->cap_spaces[idx.cspace] == nullptr) {
        log_error("fetch_cap: 对应的CSpace不存在");
        return nullptr;
    }

    if (idx.cindex < 0 || idx.cindex >= CSPACE_ITEMS) {
        log_error("fetch_cap: CIndex超出范围");
        return nullptr;
    }

    CSpace *space   = &pcb->cap_spaces[idx.cspace];
    Capability *cap = (*space)[idx.cindex];
    if (cap == nullptr) {
        log_error("fetch_cap: CIndex对应的Capability不存在");
        return nullptr;
    }
    return cap;
}

CapIdx lookup_slot(PCB *pcb) {
    if (pcb == nullptr) {
        log_error("lookup_slot: pcb不能为空!");
        return INVALID_CAP_IDX;
    }

    if (pcb->cap_spaces == nullptr) {
        log_error("lookup_slot: PCB块中无CSpaces");
        return INVALID_CAP_IDX;
    }

    // 遍历CSpaces
    for (int i = 0; i < PROC_CSPACES; i++) {
        if (pcb->cap_spaces[i] == nullptr) {
            // 创建新的CapSpace
            pcb->cap_spaces[i] = new_cspace();
        }
        CSpace *space = &pcb->cap_spaces[i];
        for (int j = 0; j < CSPACE_ITEMS; j++) {
            // 跳过INVALID_CAP_IDX
            if (i + j == 0) {
                continue;
            }
            // 找到空位
            if ((*space)[j] == nullptr) {
                return (CapIdx){.cspace = i, .cindex = j};
            }
        }
    }

    log_error("lookup_slot: PCB中槽位已满!");
    return INVALID_CAP_IDX;
}

/**
 * @brief 向pcb中插入cap, 插入到指定位置
 *
 * @param pcb 进程控制块
 * @param cap 能力
 * @param idx 指定位置的能力索引
 * @return CapIdx 能力索引
 */
CapIdx insert_cap_at(PCB *pcb, Capability *cap, CapIdx idx) {
    if (cap == nullptr) {
        log_error("insert_cap_at: cap不能为空!");
        return INVALID_CAP_IDX;
    }

    if (pcb == nullptr) {
        log_error("insert_cap_at: pcb不能为空!");
        return INVALID_CAP_IDX;
    }

    if (pcb->cap_spaces == nullptr) {
        log_error("insert_cap_at: PCB块中无CSpaces");
        return INVALID_CAP_IDX;
    }

    // 检查idx是否合法
    if (idx.cspace < 0 || idx.cspace >= PROC_CSPACES) {
        log_error("insert_cap_at: CSpace超出范围");
        return INVALID_CAP_IDX;
    }

    if (idx.cindex < 0 || idx.cindex >= CSPACE_ITEMS) {
        log_error("insert_cap_at: CIndex超出范围");
        return INVALID_CAP_IDX;
    }

    // 若对应CSpace不存在, 则创建
    if (pcb->cap_spaces[idx.cspace] == nullptr) {
        pcb->cap_spaces[idx.cspace] = new_cspace();
    }

    CSpace *space = &pcb->cap_spaces[idx.cspace];
    // 检查指定位置是否已被占用
    if ((*space)[idx.cindex] != nullptr) {
        log_error("insert_cap_at: 指定位置已被占用");
        return INVALID_CAP_IDX;
    }

    // 插入能力
    (*space)[idx.cindex] = cap;
    cap->idx             = idx;
    // 插入链表
    list_push_back(cap, CAPABILITY_LIST(pcb));
    return idx;
}

CapIdx insert_cap(PCB *pcb, Capability *cap) {
    return insert_cap_at(pcb, cap, lookup_slot(pcb));
}

/**
 * @brief 构造能力
 *
 * @param p 需要构造能力的进程控制块
 * @param type 能力类型
 * @param cap_data 能力数据
 * @param cap_priv 能力权限
 * @param attached_priv 附加权限
 * @return Capability* 能力索引
 */
static Capability *__create_cap(PCB *p, CapType type, void *cap_data,
                                const qword cap_priv, void *attached_priv) {
    if (p == nullptr) {
        log_error("create_cap: PCB不能为空!");
        return nullptr;
    }
    if (cap_data == nullptr) {
        log_error("create_cap: 能力数据为空!");
        return nullptr;
    }
    // 构造能力对象
    Capability *cap = (Capability *)kmalloc(sizeof(Capability));
    memset(cap, 0, sizeof(Capability));
    // 初始化派生能力链表
    list_init(CHILDREN_CAP_LIST(cap));
    // 设置能力属性
    cap->type          = type;
    cap->pcb           = p;
    cap->cap_data      = cap_data;
    cap->cap_priv      = cap_priv;
    cap->attached_priv = attached_priv;
    return cap;
}

CapIdx create_cap_at(PCB *p, CapType type, void *cap_data, const qword cap_priv,
                     void *attached_priv, CapIdx idx) {
    // 构造能力对象
    Capability *cap = __create_cap(p, type, cap_data, cap_priv, attached_priv);
    if (cap == nullptr) {
        return INVALID_CAP_IDX;
    }

    // 插入能力到PCB
    CapIdx ret = insert_cap_at(p, cap, idx);
    if (CAPIDX_INVALID(ret)) {
        kfree(cap);
        return INVALID_CAP_IDX;
    }
    return ret;
}

CapIdx create_cap(PCB *p, CapType type, void *cap_data, const qword cap_priv,
                  void *attached_priv) {
    return create_cap_at(p, type, cap_data, cap_priv, attached_priv,
                         lookup_slot(p));
}

/**
 * @brief 派生能力
 *
 * @param dst_p 目标进程控制块
 * @param parent 父能力
 * @param cap_priv 子能力权限
 * @param attached_priv 附加权限
 * @return Capability* 能力
 */
static Capability *__derive_cap(PCB *dst_p, Capability *parent, qword cap_priv,
                                void *attached_priv) {
    // 进行权限检查
    if (!derivable(parent->cap_priv, cap_priv) ||
        !derivable(parent->cap_priv, CAP_PRIV_DERIVE))
    {
        log_error("__derive_cap: 父能力权限不包含子能力权限, 无法派生!");
        return nullptr;
    }

    // 构造能力对象
    return __create_cap(dst_p, parent->type, parent->cap_data, cap_priv,
                        attached_priv);
}

CapIdx derive_cap_at(PCB *p, Capability *parent, qword cap_priv,
                     void *attached_priv, CapIdx idx) {
    if (parent == nullptr) {
        log_error("derive_cap_at: 父能力不能为空!");
        return INVALID_CAP_IDX;
    }

    // 派生能力对象
    Capability *cap = __derive_cap(p, parent, cap_priv, attached_priv);
    if (cap == nullptr) {
        return INVALID_CAP_IDX;
    }

    // 插入能力到PCB
    CapIdx ret = insert_cap_at(p, cap, idx);
    if (CAPIDX_INVALID(ret)) {
        kfree(cap);
        return INVALID_CAP_IDX;
    }

    // 将新能力加入到父能力的派生链表中
    cap->parent = parent;
    list_push_back(cap, CHILDREN_CAP_LIST(parent));
    return idx;
}

CapIdx derive_cap(PCB *p, Capability *parent, qword cap_priv,
                  void *attached_priv) {
    return derive_cap_at(p, parent, cap_priv, attached_priv, lookup_slot(p));
}

bool degrade_cap(PCB *p, Capability *cap, const qword new_priv) {
    // 附加权限由各个能力类型自行检查

    if (!derivable(cap->cap_priv, new_priv)) {
        log_error("degrade_cap: 父能力权限不包含子能力权限, 无法降级!");
        return false;
    }

    // 复制新能力权限
    cap->cap_priv = new_priv;
    return true;
}

const char *cap_type_to_string(CapType type) {
    switch (type) {
        case CAP_TYPE_NUL: return "CAP_TYPE_NUL";
        case CAP_TYPE_PCB: return "CAP_TYPE_PCB";
        case CAP_TYPE_TCB: return "CAP_TYPE_TCB";
        case CAP_TYPE_MEM: return "CAP_TYPE_MEM";
        case CAP_TYPE_NOT: return "CAP_TYPE_NOT";
        default:           return "Invalid type";
    }
}