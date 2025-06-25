/*
 * Copyright 2021, Breakaway Consulting Pty. Ltd.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define __thread
#include <sel4/sel4.h>

#include <microkit.h>

#define INPUT_CAP 1
#define REPLY_CAP 4

#define PD_MASK 0xff
#define CHANNEL_MASK 0x3f

/* All globals are prefixed with microkit_* to avoid clashes with user defined globals. */

bool microkit_passive;
char microkit_name[MICROKIT_PD_NAME_LENGTH];
/* We use seL4 typedefs as this variable is exposed to the libmicrokit header
 * and we do not want to rely on compiler built-in defines. */
seL4_Bool microkit_have_signal = seL4_False;
seL4_CPtr microkit_signal_cap;
seL4_MessageInfo_t microkit_signal_msg;

seL4_Word microkit_irqs;
seL4_Word microkit_notifications;
seL4_Word microkit_pps;

extern seL4_IPCBuffer __sel4_ipc_buffer_obj;

seL4_IPCBuffer *__sel4_ipc_buffer = &__sel4_ipc_buffer_obj;

extern const void (*const __init_array_start [])(void);
extern const void (*const __init_array_end [])(void);

__attribute__((weak)) microkit_msginfo protected(microkit_channel ch, microkit_msginfo msginfo)
{
    microkit_dbg_puts(microkit_name);
    microkit_dbg_puts(" is missing the 'protected' entry point\n");
    microkit_internal_crash(0);
    return seL4_MessageInfo_new(0, 0, 0, 0);
}

__attribute__((weak)) seL4_Bool fault(microkit_child child, microkit_msginfo msginfo, microkit_msginfo *reply_msginfo)
{
    microkit_dbg_puts(microkit_name);
    microkit_dbg_puts(" is missing the 'fault' entry point\n");
    microkit_internal_crash(0);
    return seL4_False;
}

static void run_init_funcs(void)
{
    size_t count = __init_array_end - __init_array_start;
    for (size_t i = 0; i < count; i++) {
        __init_array_start[i]();
    }
}

extern void handler_loop();

extern void *microkit_cml_heap;
extern void *microkit_cml_stack;
extern void *microkit_cml_stackend;
extern void microkit_cml_main(void);

static char microkit_cml_memory[1024*20];

void microkit_cml_exit(int arg) {
    microkit_dbg_puts("ERROR! We should not be getting here\n");
}

void microkit_cml_err(int arg) {
    if (arg == 3) {
        microkit_dbg_puts("Memory not ready for entry. You may have not run the init code yet, or be trying to enter during an FFI call.\n");
    }
    microkit_cml_exit(arg);
}

/* Need to come up with a replacement for this clear cache function.
    Might be worth testing just flushing the entire l1 cache,
    but might cause issues with returning to this file
*/
void microkit_cml_clear() {
    microkit_dbg_puts("Trying to clear cache\n");
}

void microkit_init_pancake_mem() {
    unsigned long microkit_cml_heap_sz = 1024*10;
    unsigned long microkit_cml_stack_sz = 1024*10;
    microkit_cml_heap = microkit_cml_memory;
    microkit_cml_stack = microkit_cml_heap + microkit_cml_heap_sz;
    microkit_cml_stackend = microkit_cml_stack + microkit_cml_stack_sz;
}

/* FFI function to check if we have a signal to send */
void ffimicrokit_have_signal(unsigned char *c, long clen, unsigned char *a, long alen) {
    uintptr_t *pnk_mem = (uintptr_t *)microkit_cml_heap;
    pnk_mem[clen] = microkit_have_signal;
}

/* FFI function for seL4_Recv - writes tag and badge to memory */
void ffimicrokit_recv(unsigned char *c, long clen, unsigned char *a, long alen) {
    seL4_Word badge;
    seL4_MessageInfo_t tag = seL4_Recv(INPUT_CAP, &badge, REPLY_CAP);
    
    uintptr_t *pnk_mem = (uintptr_t *)microkit_cml_heap;
    
    pnk_mem[clen] = badge;
    pnk_mem[alen] = *(seL4_Word*)&tag;  // Just cast it directly
}

/* FFI function for seL4_ReplyRecv - writes tag and badge to memory */
void ffimicrokit_reply_recv(unsigned char *c, long clen, unsigned char *a, long alen) {
    seL4_Word badge;
    uintptr_t *pnk_mem = (uintptr_t *)microkit_cml_heap;
    
    // clen = reply_tag_addr, a = badge_addr, alen = tag_addr
    // Read reply_tag from memory
    seL4_MessageInfo_t reply_tag = *(seL4_MessageInfo_t*)&pnk_mem[clen];
    seL4_MessageInfo_t tag = seL4_ReplyRecv(INPUT_CAP, reply_tag, &badge, REPLY_CAP);
    
    // Write results to memory
    pnk_mem[(uintptr_t)a] = badge;     // badge to BADGE_ADDR (500)
    pnk_mem[alen] = *(seL4_Word*)&tag; // tag to TAG_ADDR (501)
}

/* FFI function for seL4_NBSendRecv - writes tag and badge to memory */
void ffimicrokit_nb_send_recv(unsigned char *c, long clen, unsigned char *a, long alen) {
    seL4_Word badge;
    seL4_MessageInfo_t tag = seL4_NBSendRecv(microkit_signal_cap, microkit_signal_msg, INPUT_CAP, &badge, REPLY_CAP);
    
    uintptr_t *pnk_mem = (uintptr_t *)microkit_cml_heap;
    
    pnk_mem[clen] = badge;
    pnk_mem[alen] = *(seL4_Word*)&tag;
    
    // Clear the signal flag
    microkit_have_signal = seL4_False;
}

/* FFI function to call fault handler - writes reply info to memory */
void ffimicrokit_fault_handler(unsigned char *c, long clen, unsigned char *a, long alen) {
    uintptr_t *pnk_mem = (uintptr_t *)microkit_cml_heap;
    
    // c = reply_tag_addr (504), clen = child, a = should_reply_addr (503), alen = tag_addr (501)
    microkit_child child = clen;
    seL4_MessageInfo_t msginfo = *(seL4_MessageInfo_t*)&pnk_mem[alen];  // Read tag from TAG_ADDR
    seL4_MessageInfo_t reply_tag;
    
    seL4_Bool should_reply = fault(child, msginfo, &reply_tag);
    
    pnk_mem[(uintptr_t)c] = *(seL4_Word*)&reply_tag;  // Write reply_tag to REPLY_TAG_ADDR
    pnk_mem[(uintptr_t)a] = should_reply;             // Write should_reply to SHOULD_REPLY_ADDR
}

/* FFI function to call protected handler - writes reply tag to memory */
void ffimicrokit_protected_handler(unsigned char *c, long clen, unsigned char *a, long alen) {
    uintptr_t *pnk_mem = (uintptr_t *)microkit_cml_heap;
    
    // c = reply_tag_addr (504), clen = channel, a = unused (0), alen = tag_addr (501)
    microkit_channel channel = clen;
    seL4_MessageInfo_t msginfo = *(seL4_MessageInfo_t*)&pnk_mem[alen];  // Read tag from TAG_ADDR
    
    seL4_MessageInfo_t reply_tag = protected(channel, msginfo);
    
    pnk_mem[(uintptr_t)c] = *(seL4_Word*)&reply_tag;  // Write reply_tag to REPLY_TAG_ADDR
}

/* FFI function to call notified handler */
void ffimicrokit_notified_handler(unsigned char *c, long clen, unsigned char *a, long alen) {
    notified(clen);
}

void main(void)
{
    run_init_funcs();
    init();

    microkit_init_pancake_mem();
        
    /*
     * If we are passive, now our initialisation is complete we can
     * signal the monitor to unbind our scheduling context and bind
     * it to our notification object.
     * We delay this signal so we are ready waiting on a recv() syscall
     */
    if (microkit_passive) {
        microkit_have_signal = seL4_True;
        microkit_signal_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        microkit_signal_cap = MONITOR_EP;
    }

    microkit_cml_main();

    handler_loop();
}
