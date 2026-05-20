#include <kmod/syscall.h>
#include <rpc/packet.h>
#include <rpc/session.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

constexpr sus_u32 kServiceMagic = 0x12345678;
constexpr sus_u32 kSetFunction  = 0;
constexpr sus_u32 kGetFunction  = 1;
constexpr sus_i32 kTestValue    = 114514;
constexpr size_t kScanSlots     = 16;

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
        printf("test_rpc_client: 预期找到一个endpoint, 实际=%u\n", count);
        exit(-1);
    }

    return found;
}

static void fail(const char *msg) {
    printf("test_rpc_client: %s\n", msg);
    exit(-1);
}

int kmod_main() {
    printf("test_rpc_client: start pid=%u\n", sys_getpid(__pcb_cap));
    CapIdx endpoint = find_unique_endpoint_cap();

    rpc::Client client(endpoint, "sample_service", kServiceMagic);
    auto session_res = client.start();
    if (!session_res.has_value()) {
        fail("启动RPC session失败");
    }
    auto &session = session_res.value().get();

    rpc::ByteBuffer set_args(sizeof(kTestValue));
    auto write_res = set_args.write(kTestValue);
    if (!write_res.has_value()) {
        fail("写入set参数失败");
    }

    std::vector<sus_u32> set_types;
    set_types.push_back(rpc::prim_typeid(rpc::PrimitiveTypeId::i32));
    auto set_res = session.call(
        kSetFunction, set_types, set_args,
        rpc::prim_typeid(rpc::PrimitiveTypeId::void_type));
    if (!set_res.has_value()) {
        fail("RPC set(i32)调用失败");
    }
    if (set_res.value().retbuf.size() != 0) {
        fail("RPC set(i32)返回值长度错误");
    }
    printf("test_rpc_client: set(%d) ok\n", kTestValue);

    rpc::ByteBuffer get_args(0);
    std::vector<sus_u32> get_types;
    auto get_res = session.call(
        kGetFunction, get_types, get_args,
        rpc::prim_typeid(rpc::PrimitiveTypeId::i32));
    if (!get_res.has_value()) {
        fail("RPC get()调用失败");
    }

    auto value_res = get_res.value().retbuf.read<sus_i32>();
    if (!value_res.has_value()) {
        fail("读取RPC get()返回值失败");
    }
    sus_i32 value = value_res.value();
    printf("test_rpc_client: get()=%d expected=%d\n", value, kTestValue);
    if (value != kTestValue) {
        fail("RPC get()返回值不匹配");
    }

    auto close_res = session.close();
    if (!close_res.has_value()) {
        fail("关闭RPC session失败");
    }

    printf("test_rpc_client: RPC测试完成\n");
    exit(-1);
    return 0;
}
