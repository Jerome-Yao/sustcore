#include <kmod/syscall.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr CapIdx kEndpointCap = cap::make(1, 3);
constexpr uint64_t kValueV    = 0x123456789abcdef0ULL;
constexpr size_t kRepeatCount = 10;

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
    printf("test1: start\n");
    printf("test1: pid=%u\n", sys_getpid(__pcb_cap));

    if (!sys_create_endpoint(kEndpointCap)) {
        printf("test1: 创建端点失败!\n");
        while (true) {
        }
    }

    CapIdx initial_caps[] = {kEndpointCap};
    CapIdx test2_pcb =
        sys_create_process("/initrd/test2.mod", (CapIdx *)initial_caps, 1, 3);
    if (test2_pcb == cap::error) {
        printf("test1: 创建 test2 失败!\n");
        while (true) {
        }
    }

    uint64_t k = recv_u64(kEndpointCap, "test1");
    printf("test1: 收到密钥 K=0x%016lx\n", k);

    for (size_t round = 0; round < kRepeatCount; ++round) {
        uint64_t v = kValueV + round;
        uint64_t c = k ^ v;
        printf("test1: 第%u轮 V=0x%016lx C=0x%016lx\n", round, v, c);
        sys_send_msg(kEndpointCap, &c, sizeof(c), nullptr, 0);
    }

    while (true) {
    }
    return 0;
}
