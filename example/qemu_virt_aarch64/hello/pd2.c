/*
 * Copyright 2021, Breakaway Consulting Pty. Ltd.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <stdint.h>
#include <microkit.h>

#define PD_HELLO_CH 1

#define PSCI_CPU_OFF 0x84000002

void init(void)
{
    microkit_dbg_puts("[PD 2]: hello, world\n");
}

void notified(microkit_channel ch)
{
    switch (ch) {
        case PD_HELLO_CH: {
            microkit_dbg_puts("[PD 2]: received notification from PD 1 and is now suiciding\n");

            seL4_ARM_SMCContext args = {0};
            seL4_ARM_SMCContext response = {0};
            args.x0 = PSCI_CPU_OFF;

            microkit_arm_smc_call(&args, &response);
            break;
        }
    }
}
