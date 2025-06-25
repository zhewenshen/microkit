#
# Copyright 2021, Breakaway Consulting Pty. Ltd.
#
# SPDX-License-Identifier: BSD-2-Clause
#
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must be specified)
endif

ifeq ($(strip $(ARCH)),)
$(error ARCH must be specified)
endif

ifeq ($(strip $(TOOLCHAIN)),)
$(error TOOLCHAIN must be specified)
endif

ifeq ($(ARCH),aarch64)
	ASM_FLAGS := -mcpu=$(GCC_CPU)
	CFLAGS_AARCH64 := -mcpu=$(GCC_CPU)
	CFLAGS_ARCH := $(CFLAGS_AARCH64)
	ARCH_DIR := aarch64
else ifeq ($(ARCH),riscv64)
	ASM_FLAGS := -march=rv64imafdc_zicsr_zifencei -mabi=lp64d
	CFLAGS_RISCV64 := -mcmodel=medany -march=rv64imafdc_zicsr_zifencei -mabi=lp64d
	CFLAGS_ARCH := $(CFLAGS_RISCV64)
	ARCH_DIR := riscv
endif

CFLAGS := -std=gnu11 \
		  -g -O3 -nostdlib \
		  -ffreestanding \
		  -Wall -Wno-maybe-uninitialized \
		  -Wno-unused-function -Werror \
		  -Iinclude -I$(SEL4_SDK)/include \
		  $(CFLAGS_ARCH)

MAIN_PNK = main.🥞 util.🥞

CC_IS_CLANG := $(shell $(TOOLCHAIN)gcc --version 2>/dev/null | grep -q clang && echo yes || echo no)

ifeq ($(CC_IS_CLANG),yes)
    TARGET_FLAG := -target aarch64-none-elf
else
    TARGET_FLAG :=
endif

LIBS := libmicrokit.a
OBJS := main.o crt0.o dbg.o pancake_ffi.o

.DEFAULT_GOAL := all

$(BUILD_DIR)/%.o : src/$(ARCH_DIR)/%.S
	$(TOOLCHAIN)gcc -x assembler-with-cpp -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o : src/$(ARCH_DIR)/%.s
	$(TOOLCHAIN)as -g $(ASM_FLAGS) $< -o $@

$(BUILD_DIR)/%.o : src/%.c
	$(TOOLCHAIN)gcc -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/pancake_ffi.o: pancake_ffi.c
	$(TOOLCHAIN)gcc -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/main_pnk.S: $(MAIN_PNK)
	cat $(MAIN_PNK) | cpp -P | cake --target=arm8 --pancake --main_return=true > $@

$(BUILD_DIR)/main_pnk.o: $(BUILD_DIR)/main_pnk.S
	$(TOOLCHAIN)gcc -c $(ASM_FLAGS) $(TARGET_FLAG) $< -o $@

$(BUILD_DIR)/main.o: $(BUILD_DIR)/main_pnk.o
	cp $< $@

LIB = $(addprefix $(BUILD_DIR)/, $(LIBS))

all: $(LIB)

$(LIB): $(addprefix $(BUILD_DIR)/, $(OBJS))
	$(TOOLCHAIN)ar -rv $@ $^
