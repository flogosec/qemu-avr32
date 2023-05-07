/*
 * QEMU AVR CPU
 *
 * Copyright (c) 2022-2023 Florian Göhler, Johannes Willbold
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>
 */
#ifndef QEMU_AVR32A_CPU_H
#define QEMU_AVR32A_CPU_H

#include "cpu-qom.h"
#include "exec/cpu-defs.h"

#define AVR32_EXP 0x100
#define AVR32_EXP_S    AVR32_EXP | 0x30

#define AVR32A_REG_PAGE_SIZE 16 // r0 - r12 + LR + SP + PC
#define AVR32A_PC_REG 15
#define AVR32A_LR_REG 14
#define AVR32A_SP_REG 13
#define AVR32A_SYS_REG 256



struct AVR32ACPUDef {
    const char* name;
    const char* parent_microarch; // AVR32A or AVR32B
    size_t core_type; // AVR32_EXP
    size_t series_type;
    size_t clock_speed;
    bool audio; // MCUs with 'AU' suffix
    bool aes;  // MCUs with 'S' suffix
};


#define AVR32A_CPU_TYPE_SUFFIX "-" TYPE_AVR32A_CPU
#define AVR32A_CPU_TYPE_NAME(name) (name AVR32A_CPU_TYPE_SUFFIX)
#define CPU_RESOLVING_TYPE TYPE_AVR32A_CPU

// Global Interrupt Mask
#define AVR32_GM_FLAG(sr)       (sr & 0x10000) >> 16

#define AVR32_EXTENDED_INSTR_FORMAT_MASK 0b1110000000000000
#define AVR32_EXTENDED_INSTR_FORMAT_MASK_LE 0b11100000

static const char avr32_cpu_r_names[AVR32A_REG_PAGE_SIZE][8] = {
        "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
        "r8",  "r9",  "r10",  "r11",  "r12",  "SP",  "LR",  "PC",
};

static const char avr32_cpu_sr_flag_names[32][8] = {
        "sregC", "sregZ", "sregN", "sregV", "sregQ", "sregL",
        "sreg6", "sreg7", "sreg8", "sreg9", "sreg10", "sreg11","sreg12","sreg13",
        "sregT", "sregR",
        "sregGM","sregI0M","sregI1M", "sregI2M", "sregI3M","sregEM","sregM0",
        "sregM1",
        "sregM2", "sreg25","sregD","sregDM","sregJ","sregH", "sreg30", "sregSS"
};

typedef struct CPUArchState {
    // Status Register
    uint sr;
    uint pc_w;

    uint32_t sflags[32];

    // Register File Registers
    uint32_t r[AVR32A_REG_PAGE_SIZE]; // 32 bits each

    //System registers
    uint32_t sysr[AVR32A_SYS_REG];

    //interrupt source
    int intsrc;
    int intlevel;
    uint64_t autovector;
    int isInInterrupt;

} CPUAVR32AState;

struct ArchCPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/

    CPUNegativeOffsetState neg;
    CPUAVR32AState env;
};


int avr32_print_insn(bfd_vma addr, disassemble_info *info);


static inline int cpu_interrupts_enabled(CPUAVR32AState* env)
{
    return AVR32_GM_FLAG(env->sr);
}

static inline int cpu_mmu_index(CPUAVR32AState *env, bool ifetch)
{
    // There is only one MMU, so that should be fine
    return 0;
}

static inline void cpu_get_tb_cpu_state(CPUAVR32AState *env, target_ulong *pc,
                                        target_ulong *cs_base, uint32_t *pflags)
{
    *pc = env->r[AVR32A_PC_REG];
    *cs_base = 0;
    *pflags = 0;
}

void avr32_tcg_init(void);
bool avr32_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                        MMUAccessType access_type, int mmu_idx,
                        bool probe, uintptr_t retaddr);
void avr32_cpu_do_interrupt(CPUState *cpu);
void avr32_cpu_set_int(void *opaque, int irq, int level);
hwaddr avr32_cpu_get_phys_page_debug(CPUState *cs, vaddr addr);
int avr32_cpu_memory_rw_debug(CPUState *cs, vaddr addr, uint8_t *buf, int len, bool is_write);

void avr32_cpu_synchronize_from_tb(CPUState *cs, const TranslationBlock *tb);

#include "exec/cpu-all.h"

#endif /* !defined (QEMU_AVR32A_CPU_H) */
