#include <kmod/syscall.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr uint64_t kValueK    = 0xfedcba9876543210ULL;
constexpr size_t kRepeatCount = 10;
constexpr size_t kScanSlots   = 16;

// test2 模块: 从 test1 接收一个值, 进行异或运算后再发回去, 重复多轮
// 这里寻找能力空间中唯一的端点能力, 从而与 test1 进行通信
static CapIdx find_unique_endpoint_cap() {
    CapIdx found = cap::null;
    size_t count = 0;

    for (size_t slot = 0; slot < kScanSlots; ++slot) {
        CapIdx candidate = cap::make(1, slot);
        CapInfo info{};
        if (!lookup_cap(candidate, &info) || info.type != PayloadType::ENDPOINT)
        {
            continue;
        }

        found = candidate;
        ++count;
    }

    if (count != 1) {
        printf("test2: 预期找到一个端点 capability, 但是找到了 %u 个\n", count);
        while (true) {
        }
    }

    return found;
}

static uint64_t recv_u64(CapIdx endpoint, const char *tag) {
    uint64_t value = 0;
    size_t msgsz   = 0;
    size_t capsz   = 0;

    sys_recv_msg(endpoint, &value, &msgsz, nullptr, &capsz);
    if (msgsz != sizeof(value) || capsz != 0) {
        printf("%s: 无效的消息 msgsz=%u capsz=%u\n", tag, msgsz, capsz);
        while (true) {
        }
    }

    return value;
}

int kmod_main() {
    printf("test2 启动! \n");
    printf("test2: pid=%u\n", sys_getpid(__pcb_cap));

    CapIdx endpoint_cap = find_unique_endpoint_cap();

    printf("test2: 发送密钥 K=0x%016lx\n", kValueK);
    sys_send_msg(endpoint_cap, &kValueK, sizeof(kValueK), nullptr, 0);

    for (size_t round = 0; round < kRepeatCount; ++round) {
        uint64_t c = recv_u64(endpoint_cap, "test2");
        uint64_t v = c ^ kValueK;
        printf("test2: 第%u轮 K=0x%016lx C=0x%016lx V=0x%016lx\n", round,
               kValueK, c, v);
    }

    while (true) {
    }
    return 0;
}
