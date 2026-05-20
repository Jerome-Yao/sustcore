#include <kmod/syscall.h>
#include <rpc/packet.h>
#include <rpc/session.h>
#include <sustcore/msg.h>

#include <cstdio>
#include <string>

class SampleService : public rpc::ServerSession {
private:
    int _x = 0;

public:
    SampleService(rpc::Server &server, sus_u32 session_number)
        : rpc::ServerSession(server, session_number) {}

    Result<void> handle_call(const rpc::CallPacket &msg,
                             CallResult &result) override {
        switch (msg.function_id) {
            case 0: {
                // check types
                if (msg.types.size() != 1) {
                    result.is_error = true;
                    void_return();
                }
                if (msg.types[0] != rpc::prim_typeid(rpc::PrimitiveTypeId::i32))
                {
                    result.is_error = true;
                    void_return();
                }

                // check if argument buffer is valid
                if (msg.argbuf.size() != sizeof(sus_i32)) {
                    result.is_error = true;
                    void_return();
                }

                auto read_res = msg.argbuf.read<sus_i32>();
                propagate(read_res);
                _x              = read_res.value();
                result.is_error = false;
                result.return_type =
                    rpc::prim_typeid(rpc::PrimitiveTypeId::void_type);
                void_return();
            }
            case 1: {
                // check types
                if (msg.types.size() != 0) {
                    result.is_error = true;
                    void_return();
                }
                // check if argument buffer is valid
                if (msg.argbuf.size() != 0) {
                    result.is_error = true;
                    void_return();
                }

                result.is_error = false;
                result.return_type =
                    rpc::prim_typeid(rpc::PrimitiveTypeId::i32);
                auto write_res = result.retbuf.write(_x);
                propagate(write_res);
                void_return();
            }
        };
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
};

class SampleServer : public rpc::Server {
public:
    SampleServer(CapIdx endpoint)
        : rpc::Server(endpoint, "sample_service", 0x12345678) {}

    rpc::ServerSession *create_session(sus_u32 session_number) override {
        return new SampleService(*this, session_number);
    }
};

int kmod_main() {
    CapIdx endpoint = sys_endpoint_create();
    if (endpoint == cap::error) {
        printf("Failed to create endpoint\n");
        exit(0);
    }

    SampleServer server(endpoint);
    printf("Server is running endpoint=%p\n", (void *)endpoint);

    CapIdx initial_caps[] = {endpoint};
    CapIdx client_pcb = sys_create_process("/initrd/test_rpc_client.mod",
                                           initial_caps, 1, SCHED_CLASS_RR);
    if (client_pcb == cap::error) {
        printf("Failed to create test_rpc_client\n");
        exit(0);
    }
    sys_cap_remove(client_pcb);
    printf("test_rpc_server: client started\n");

    while (true) {
        MsgPacket recv_msg{
            .msgsz = MAX_MSG_SIZE,
            .capsz = MAX_MSG_CAPS,
        };
        sys_endpoint_recv(endpoint, &recv_msg);
        if (rpc::is_rpc_message(recv_msg)) {
            server.handle_message(recv_msg);
        } else {
            printf("Received non-RPC message, ignoring...\n");
        }
    }

    return 0;
}
