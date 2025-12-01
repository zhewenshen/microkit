#ifndef SEL4_FFI_H
#define SEL4_FFI_H

#include <stdint.h>

typedef uint64_t seL4_Word;
typedef uint64_t seL4_CPtr;
typedef int8_t seL4_Bool;
typedef uint64_t seL4_MessageInfo_t;

#define seL4_True  1
#define seL4_False 0

extern seL4_MessageInfo_t ffi_seL4_Recv(seL4_CPtr src, seL4_Word *sender, seL4_CPtr reply);
extern seL4_MessageInfo_t ffi_seL4_ReplyRecv(seL4_CPtr src, seL4_MessageInfo_t msgInfo, seL4_Word *sender, seL4_CPtr reply);
extern seL4_MessageInfo_t ffi_seL4_NBSendRecv(seL4_CPtr dest, seL4_MessageInfo_t msgInfo, seL4_CPtr src, seL4_Word *sender, seL4_CPtr reply);

#endif
