/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Pancake Foreign Function Interface for microkit
 */

#include <sel4/sel4.h>
#include <microkit.h>

void ffimicrokit_hello_pancake(char a, char b, char c, char d) {
    microkit_dbg_puts("Hello from microkit pancake!\n");
}
