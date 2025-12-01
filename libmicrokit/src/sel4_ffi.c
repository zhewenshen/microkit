#include <stdint.h>
#define __thread
#include <sel4/sel4.h>

uint64_t ffi_seL4_Recv(seL4_CPtr src, seL4_Word *sender, seL4_CPtr reply)
{
    seL4_MessageInfo_t info = seL4_Recv(src, sender, reply);
    return info.words[0];
}

uint64_t ffi_seL4_ReplyRecv(seL4_CPtr src, uint64_t msgInfo, seL4_Word *sender, seL4_CPtr reply)
{
    seL4_MessageInfo_t info = { .words = { msgInfo } };
    seL4_MessageInfo_t ret = seL4_ReplyRecv(src, info, sender, reply);
    return ret.words[0];
}

uint64_t ffi_seL4_NBSendRecv(seL4_CPtr dest, uint64_t msgInfo, seL4_CPtr src, seL4_Word *sender, seL4_CPtr reply)
{
    seL4_MessageInfo_t info = { .words = { msgInfo } };
    seL4_MessageInfo_t ret = seL4_NBSendRecv(dest, info, src, sender, reply);
    return ret.words[0];
}
