#include <kmod/syscall.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

constexpr CapIdx kCallEndpointCap = cap::make(1, 4);
constexpr uint64_t kOpEncrypt     = 1;
constexpr uint64_t kOpDecrypt     = 2;
constexpr uint64_t kKey           = 0xfedcba9876543210ULL;
constexpr uint64_t kValue         = 0x123456789abcdef0ULL;
constexpr size_t kScanSlots       = 16;

struct CallRequest {
    uint64_t op;
    uint64_t key;
    uint64_t value;
};

static CapIdx find_unique_endpoint_cap() {
    CapIdx found = cap::null;
    size_t count = 0;

    for (size_t slot = 0; slot < kScanSlots; ++slot) {
        CapIdx candidate = cap::make(0, slot);
        CapInfo info{};
        if (!sys_cap_lookup(candidate, &info) ||
            info.type != PayloadType::ENDPOINT)
        {
            continue;
        }

        found = candidate;
        ++count;
    }

    if (count != 1) {
        printf("test_call_user: 预期找到一个endpoint, 实际=%u\n", count);
        exit(-1);
    }

    return found;
}

static uint64_t call_service(CapIdx endpoint, uint64_t op, uint64_t value) {
    CallRequest request{
        .op    = op,
        .key   = kKey,
        .value = value,
    };
    MsgPacket send_packet{
        .msgsz = sizeof(request),
        .capsz = 0,
    };
    memcpy(send_packet.msgbuf, &request, sizeof(request));

    uint64_t reply = 0;
    MsgPacket reply_packet{
        .msgsz = sizeof(reply),
        .capsz = 0,
    };

    endpoint_call(endpoint, &send_packet, &reply_packet);
    if (reply_packet.msgsz != sizeof(reply) || reply_packet.capsz != 0) {
        printf("test_call_user: reply格式错误 msgsz=%u capsz=%u\n",
               reply_packet.msgsz, reply_packet.capsz);
        exit(-1);
    }
    memcpy(&reply, reply_packet.msgbuf, sizeof(reply));
    return reply;
}

int kmod_main() {
    printf("test_call_user: start pid=%u\n", sys_getpid(__pcb_cap));
    CapIdx endpoint = find_unique_endpoint_cap();

    uint64_t encrypted       = call_service(endpoint, kOpEncrypt, kValue);
    uint64_t expected_cipher = kKey ^ kValue;
    printf("test_call_user: encrypt V=0x%016lx C=0x%016lx\n", kValue,
           encrypted);
    if (encrypted != expected_cipher) {
        printf("test_call_user: encrypt结果错误 expected=0x%016lx\n",
               expected_cipher);
        exit(-1);
    }

    uint64_t decrypted = call_service(endpoint, kOpDecrypt, encrypted);
    printf("test_call_user: decrypt C=0x%016lx V=0x%016lx\n", encrypted,
           decrypted);
    if (decrypted != kValue) {
        printf("test_call_user: decrypt结果错误 expected=0x%016lx\n", kValue);
        exit(-1);
    }

    printf("test_call_user: endpoint_call/endpoint_reply测试完成\n");
    exit(0);
    return 0;
}
