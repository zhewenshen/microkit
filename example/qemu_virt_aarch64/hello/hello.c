#include <stdint.h>
#include <microkit.h>

/* Definitions for the PL011 UART.
 * Adjust the base address as required.
 */
#define UART_IRQ_CH    1
#define PD_2_CH        2
uintptr_t uart_base_vaddr;

#define RHR_MASK               0b111111111
#define UARTDR                 0x000
#define UARTFR                 0x018
#define UARTIMSC               0x038
#define UARTICR                0x044
#define PL011_UARTFR_TXFF      (1 << 5)
#define PL011_UARTFR_RXFE      (1 << 4)

#define REG_PTR(base, offset)  ((volatile uint32_t *)((base) + (offset)))

#define PSCI_CPU_OFF       0x84000002
#define PSCI_VERSION_FID   0x84000000

static inline void uart_init(void) {
    *REG_PTR(uart_base_vaddr, UARTIMSC) = 0x50;
}

static inline int uart_get_char(void) {
    int ch = 0;
    if ((*REG_PTR(uart_base_vaddr, UARTFR) & PL011_UARTFR_RXFE) == 0) {
        ch = *REG_PTR(uart_base_vaddr, UARTDR) & RHR_MASK;
    }
    switch (ch) {
    case '\n':
        ch = '\r';
        break;
    case 8:
        ch = 0x7f;
        break;
    }
    return ch;
}

static inline void uart_put_char(int ch) {
    while ((*REG_PTR(uart_base_vaddr, UARTFR) & PL011_UARTFR_TXFF) != 0);
    *REG_PTR(uart_base_vaddr, UARTDR) = ch;
    if (ch == '\r') {
        uart_put_char('\n');
    }
}

static inline void uart_handle_irq(void) {
    *REG_PTR(uart_base_vaddr, UARTICR) = 0x7f0;
}

static inline void uart_put_str(char *str) {
    while (*str) {
        uart_put_char(*str);
        str++;
    }
}

void print_num(uint64_t num)
{
    char buffer[21];
    int i = 0;
    
    if (num == 0) {
        microkit_dbg_puts("0\n");
        return;
    }
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (i > 0) {
        microkit_dbg_putc(buffer[--i]);
    }
}

void init(void)
{
    microkit_dbg_puts("[PD 1]: hello, world\n");

    uart_init();

    seL4_IRQHandler_SetCore(BASE_IRQ_CAP + UART_IRQ_CH, 2);
}

void secondary_cpu_entry(void) {
    uart_put_str("secondary cpu entry\n");
    asm volatile("isb" ::: "memory");
}

static int target_cpu = 1;

void cpu_on_entry(void) {
    uart_put_str("cpu on entry\n");
}

extern void cpu_trampoline(void);

void notified(microkit_channel ch)
{
    if (ch == UART_IRQ_CH) {
        int c = uart_get_char();
        if (c == 'm') {
            microkit_dbg_puts("migrating to CPU ");
            print_num((target_cpu + 1) % 4);
            microkit_dbg_puts("\n");

            seL4_SchedControl_ConfigureFlags(
                BASE_SCHED_CONTROL_CAP + (target_cpu++ % 4),
                BASE_SCHED_CONTEXT_CAP,
                microkit_pd_period,
                microkit_pd_budget,
                microkit_pd_extra_refills,
                microkit_pd_badge,
                microkit_pd_flags
            );
        } else if (c == 'd') {
            microkit_dbg_puts("PD 1 now suiciding\n");

            seL4_ARM_SMCContext args = {0};
            seL4_ARM_SMCContext response = {0};
            args.x0 = PSCI_CPU_OFF;

            microkit_arm_smc_call(&args, &response);
        } else if (c == 'p') {
            seL4_ARM_SMCContext args = {0};
            seL4_ARM_SMCContext resp = {0};

            args.x0 = PSCI_VERSION_FID;
            microkit_arm_smc_call(&args, &resp);

            microkit_dbg_puts("PSCI version: ");
            print_num(((uint32_t)resp.x0 >> 16) & 0xFFFF);
            microkit_dbg_puts(".");
            print_num((uint32_t)resp.x0 & 0xFFFF);
            microkit_dbg_puts("\n");
        } else if (c == 'n') {
            microkit_dbg_puts("notifying PD 2\n");
            microkit_notify(PD_2_CH);
        } else if (c == 'r') {
            // microkit_dbg_puts("restarting cpu 0\n");
            // /* PSCI_CPU_ON call to restart a CPU. In this demo we use our
            //  * trampoline as the entry point.
            //  */
            #define PSCI_CPU_ON 0xC4000003
            seL4_ARM_SMCContext args = {0};
            seL4_ARM_SMCContext response = {0};
            args.x0 = PSCI_CPU_ON;
            args.x1 = 3;                         /* target CPU id (for example, core 3) */
            args.x2 = 0x00000000700022f0;
            // args.x2 = 0x8060000000ULL;           /* entry point */
            args.x3 = 0;                         /* context id (unused here) */

            microkit_arm_smc_call(&args, &response);

            // what is the response?
            microkit_dbg_puts("response: ");
            print_num(response.x0);
            microkit_dbg_puts("\n");

            // lets call cpu_trampoline directly
            // cpu_trampoline();

            // ((void (*)(void))0x8060000090ULL)();
        } else if (c == 's') {
            seL4_DebugDumpScheduler();
        } else {
            uart_put_char(c);
        }

        uart_handle_irq();
        microkit_irq_ack(ch);
    }
}
