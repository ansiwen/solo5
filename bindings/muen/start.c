/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bindings.h"
#include "../crt_init.h"
#include "solo5_version.h"

// Function to read the XCR0 register
unsigned long long read_xcr0() {
    unsigned int eax, edx;
    __asm__ __volatile__ (
        "xgetbv"
        : "=a" (eax), "=d" (edx)
        : "c" (0)
    );
    return ((unsigned long long)edx << 32) | eax;
}

// Function to set the XCR0 register to enable AVX and SSE
void set_xcr0_for_avx2() {
    unsigned long long xcrFeatureMask = 0x6;
    __asm__ __volatile__ (
        "xsetbv"
        :
        : "c" (0), "a" ((unsigned int)xcrFeatureMask), "d" ((unsigned int)(xcrFeatureMask >> 32))
    );
}

// Function to check if AVX2 is supported by the CPU
int is_avx2_supported() {
    int cpuInfo[4];
    __asm__ __volatile__ (
        "cpuid"
        : "=a" (cpuInfo[0]), "=b" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
        : "a" (0)
    );
    int nIds = cpuInfo[0];

    if (nIds >= 7) {
        __asm__ __volatile__ (
            "cpuid"
            : "=a" (cpuInfo[0]), "=b" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
            : "a" (7), "c" (0)
        );
        return (cpuInfo[1] & (1 << 5)) != 0;
    }

    return 0;
}

// Function to enable AVX2 if supported
void enable_avx2() {
    // Read and print the original state of XCR0
    unsigned long long originalXcr0 = read_xcr0();
    log(INFO, "Original XCR0 state: 0x%llx\n", originalXcr0);

    // Set XCR0 to enable AVX (bit 2) and SSE (bit 1)
    set_xcr0_for_avx2();

    // Read and print the new state of XCR0
    unsigned long long newXcr0 = read_xcr0();
    log(INFO, "New XCR0 state: 0x%llx\n", newXcr0);

    // Check if AVX2 is supported after setting XCR0
    if (!is_avx2_supported()) {
        log(INFO, "AVX2 is not supported on this system.\n");
    } else {
        log(INFO, "AVX2 is enabled!\n");
    }
}


void _start(void *arg)
{
    crt_init_ssp();
    crt_init_tls();

    static struct solo5_start_info si;

    console_init();
    cpu_init();
//    enable_avx2();
    platform_init(arg);
    si.cmdline = cmdline_parse(platform_cmdline());

    log(INFO, "            |      ___|\n");
    log(INFO, "  __|  _ \\  |  _ \\ __ \\\n");
    log(INFO, "\\__ \\ (   | | (   |  ) |\n");
    log(INFO, "____/\\___/ _|\\___/____/\n");
    log(INFO, "Solo5: Bindings version %s\n", SOLO5_VERSION);

    unsigned short control_word;
    __asm__ __volatile__("fnstcw %0" : "=m" (control_word));
    log(INFO, "FPU Control Word before fninit: 0x%04x\n", control_word);
    __asm__ __volatile__("fninit");
    __asm__ __volatile__("fnstcw %0" : "=m" (control_word));
    log(INFO, "FPU Control Word after fninit: 0x%04x\n", control_word);

    mem_init();
    time_init(arg);
    block_init(arg);
    net_init(arg);

    mem_lock_heap(&si.heap_start, &si.heap_size);
    solo5_exit(solo5_app_main(&si));
}

/*
 * Place the .interp section in this module, as it comes first in the link
 * order.
 */
DECLARE_ELF_INTERP

/*
 * The "ABI1" Solo5 ELF note is declared in this module.
 *
 * Solo5/Muen uses ABI version 2 as of Muen commit 2a64844.
 */
ABI1_NOTE_DECLARE_BEGIN
{
    .abi_target = MUEN_ABI_TARGET,
    .abi_version = 2
}
ABI1_NOTE_DECLARE_END

/*
 * Pretend that we are an OpenBSD executable. See elf_abi.h for details.
 */
DECLARE_OPENBSD_NOTE
