#include <stdbool.h>
#include <stdint.h>
#include "sel4_ffi.h"

#define INPUT_CAP    1
#define REPLY_CAP    4
#define PD_MASK      0xff
#define CHANNEL_MASK 0x3f

extern seL4_Bool microkit_have_signal;
extern seL4_CPtr microkit_signal_cap;
extern seL4_MessageInfo_t microkit_signal_msg;

extern void notified(unsigned int ch);
extern seL4_MessageInfo_t protected(unsigned int ch, seL4_MessageInfo_t msginfo);
extern seL4_Bool fault(unsigned int child, seL4_MessageInfo_t msginfo, seL4_MessageInfo_t *reply_msginfo);

void handler_loop(void)
{
    bool have_reply = false;
    seL4_MessageInfo_t reply_tag;

    for (;;) {
        seL4_Word badge;
        seL4_MessageInfo_t tag;

        if (have_reply) {
            tag = ffi_seL4_ReplyRecv(INPUT_CAP, reply_tag, &badge, REPLY_CAP);
        } else if (microkit_have_signal) {
            tag = ffi_seL4_NBSendRecv(microkit_signal_cap, microkit_signal_msg, INPUT_CAP, &badge, REPLY_CAP);
            microkit_have_signal = seL4_False;
        } else {
            tag = ffi_seL4_Recv(INPUT_CAP, &badge, REPLY_CAP);
        }

        uint64_t is_endpoint = badge >> 63;
        uint64_t is_fault = (badge >> 62) & 1;

        have_reply = false;

        if (is_fault) {
            seL4_Bool reply_to_fault = fault(badge & PD_MASK, tag, &reply_tag);
            if (reply_to_fault) {
                have_reply = true;
            }
        } else if (is_endpoint) {
            have_reply = true;
            reply_tag = protected(badge & CHANNEL_MASK, tag);
        } else {
            unsigned int idx = 0;
            do {
                if (badge & 1) {
                    notified(idx);
                }
                badge >>= 1;
                idx++;
            } while (badge != 0);
        }
    }
}
