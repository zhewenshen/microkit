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

seL4_MessageInfo_t microkit_tag = {0};
seL4_MessageInfo_t microkit_reply_tag = {0};

static uintptr_t *pnk_mem = NULL;

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

static char microkit_cml_memory[1024*2];

void microkit_cml_exit(int arg) {
    microkit_dbg_puts("ERROR! We should not be getting here\n");
}

void microkit_cml_err(int arg) {
    if (arg == 3) {
        microkit_dbg_puts("Memory not ready for entry. You may have not run the init code yet, or be trying to enter during an FFI call.\n");
    }
    microkit_cml_exit(arg);
}

void microkit_cml_clear() {
    microkit_dbg_puts("Trying to clear cache\n");
}

void microkit_init_pancake_mem() {
    unsigned long microkit_cml_heap_sz = 1024*1;
    unsigned long microkit_cml_stack_sz = 1024*1;
    microkit_cml_heap = microkit_cml_memory;
    microkit_cml_stack = microkit_cml_heap + microkit_cml_heap_sz;
    microkit_cml_stackend = microkit_cml_stack + microkit_cml_stack_sz;
    pnk_mem = (uintptr_t *)microkit_cml_heap;
}

void ffimicrokit_have_signal(unsigned char *c, long clen, unsigned char *a, long alen) {
    pnk_mem[clen] = microkit_have_signal;
}

void ffimicrokit_recv(unsigned char *c, long clen, unsigned char *a, long alen) {
    seL4_Word badge;
    microkit_tag = seL4_Recv(INPUT_CAP, &badge, REPLY_CAP);
    pnk_mem[clen] = badge;
}

void ffimicrokit_reply_recv(unsigned char *c, long clen, unsigned char *a, long alen) {
    seL4_Word badge;
    microkit_tag = seL4_ReplyRecv(INPUT_CAP, microkit_reply_tag, &badge, REPLY_CAP);
    pnk_mem[clen] = badge;
}

void ffimicrokit_nb_send_recv(unsigned char *c, long clen, unsigned char *a, long alen) {
    seL4_Word badge;
    microkit_tag = seL4_NBSendRecv(microkit_signal_cap, microkit_signal_msg, INPUT_CAP, &badge, REPLY_CAP);
    pnk_mem[clen] = badge;
    microkit_have_signal = seL4_False;
}

void ffimicrokit_fault_handler(unsigned char *c, long clen, unsigned char *a, long alen) {
    pnk_mem[alen] = fault(clen, microkit_tag, &microkit_reply_tag);
}

void ffimicrokit_protected_handler(unsigned char *c, long clen, unsigned char *a, long alen) {
    microkit_reply_tag = protected(clen, microkit_tag);
}

void ffimicrokit_notified_handler(unsigned char *c, long clen, unsigned char *a, long alen) {
    notified(clen);
}

void ffinop(unsigned char* c, long clen, unsigned char* a, long alen) {
    // do nothing
}

void main(void)
{
    microkit_init_pancake_mem();
    run_init_funcs();
    init();
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
