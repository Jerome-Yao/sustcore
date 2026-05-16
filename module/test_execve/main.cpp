#include <cstddef>
#include <cstdio>
#include <kmod/syscall.h>

constexpr size_t kSignalSyn    = 0;
constexpr size_t kSignalSynAck = 1;
constexpr size_t kSignalAck    = 2;
constexpr size_t kScanGroups   = 1;
constexpr size_t kScanSlots    = 32;

static CapIdx find_unique_notification_cap() {
    CapIdx found = cap::null;
    size_t count = 0;

    for (size_t group = 0; group < kScanGroups; ++group) {
        for (size_t slot = 0; slot < kScanSlots; ++slot) {
            CapIdx candidate = cap::make(group, slot);
            CapInfo info{};
            if (!lookup_cap(candidate, &info) ||
                info.type != PayloadType::NOTIF)
            {
                continue;
            }
            found = candidate;
            ++count;
        }
    }

    if (count != 1) {
        printf("test_execve: expected one notification cap, found %u\n",
               count);
        while (true) {
        }
    }
    return found;
}

int kmod_main() {
    printf("test_execve: start pid=%u pcb_cap=%p\n", sys_getpid(__pcb_cap),
           (void *)__pcb_cap);

    CapIdx notif_cap = find_unique_notification_cap();
    printf("test_execve: notification cap=%p\n", (void *)notif_cap);

    sys_wait_notification(notif_cap, kSignalSyn);
    printf("test_execve: SYN received\n");
    sys_unsignal_notification(notif_cap, kSignalSyn);

    sys_signal_notification(notif_cap, kSignalSynAck);
    printf("test_execve: SYN-ACK sent\n");

    sys_wait_notification(notif_cap, kSignalAck);
    printf("test_execve: ACK received\n");
    sys_unsignal_notification(notif_cap, kSignalAck);

    printf("test_execve: handshake done\n");
    sys_exit();
    while (true) {
    }
    return 0;
}
