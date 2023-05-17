/*
 * QEMU AVR CPU
 *
 * Copyright (c) 2022-2023 Florian Göhler
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
#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "exec/helper-proto.h"

static inline void raise_exception(CPUAVR32AState *env, int index,
        uintptr_t retaddr);

static inline void raise_exception(CPUAVR32AState *env, int index,
        uintptr_t retaddr)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = index;
    cpu_loop_exit_restore(cs, retaddr);
}

void helper_raise_illegal_instruction(CPUAVR32AState *env)
{
    CPUState *cs = env_cpu(env);
    raise_exception(env, 23, GETPC());
    cpu_loop_exit(cs);
}

bool avr32_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                        MMUAccessType access_type, int mmu_idx,
                        bool probe, uintptr_t retaddr)
{
    int port = 0;

    port = PAGE_READ | PAGE_EXEC | PAGE_WRITE;
    tlb_set_page(cs, address, address, port,
                 mmu_idx, TARGET_PAGE_SIZE);
    return true;
}

void avr32_cpu_do_interrupt(CPUState *cs)
{
    AVR32ACPU *cpu = AVR32A_CPU(cs);
    CPUAVR32AState *env = &cpu->env;
    uint32_t handler_addr;

    // DOCUMENTATION EXCERPT
    // When a request is accepted, the Status Register and Program Counter of the current
    // context is stored to the system stack. If the event is an INT0, INT1, INT2, or INT3,
    // registers R8-R12 and LR are also automatically stored to stack.

    if(cs->exception_index == AVR32_EXCP_IRQ) {
        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], env->r[8]);

        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], env->r[9]);

        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], env->r[10]);

        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], env->r[11]);

        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], env->r[12]);

        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], env->r[AVR32A_LR_REG]);

        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], env->r[AVR32A_PC_REG]);

        uint32_t sr = 0;
        for(int i= 0; i < 32; i++){
            sr |= (env->sflags[i] << i);
        }
        env->r[AVR32A_SP_REG] -= 4;
        cpu_stl_be_data(env, env->r[AVR32A_SP_REG], sr);

        // DOCUMENTATION EXCERPT
        // When initiating interrupt handling, the corresponding interrupt mask bit is set automatically for this and
        // lower levels in status register. E.g, if an interrupt of level 3 is approved for handling, the interrupt
        // mask bits I3M, I2M, I1M, and I0M are set in status register.
        for(int i = 0; i <= env->intlevel; ++i) {
            env->sflags[17 + i] = 1;
        }

        // TODO: Not sure why these are set, just using it for now
        env->sflags[22] = 0;
        env->sflags[23] = 1;
        env->sflags[24] = 0;

        handler_addr = env->sysr[1] + env->autovector;
        env->r[AVR32A_PC_REG] = handler_addr;

        cs->exception_index = 0;

    } else if(cs->exception_index == AVR32_EXCP_EXCP) {

        // DOCUMENTATION EXCERPT
        // When exceptions occur, both the EM and GM bits
        // are set, and the application may manually enable nested exceptions if desired by
        // clearing the appropriate bit
        env->sflags[sflagGM] = 1;
        env->sflags[sflagEM] = 1;

        printf("[avr32_cpu_do_interrupt] NOT FULLY IMPLEMENTED AVR32_EXCP_EXCP\n");

    } else {
        printf("[avr32_cpu_do_interrupt] UNKNOWN exception_index=%d\n", cs->exception_index);
        exit(-1);
    }
}

hwaddr avr32_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    return addr;
}


void helper_debug(CPUAVR32AState *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

void helper_break(CPUAVR32AState *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

int avr32_cpu_memory_rw_debug(CPUState *cs, vaddr addr, uint8_t *buf,
                            int len, bool is_write)
{
    return cpu_memory_rw_debug(cs, addr, buf, len, is_write);
}
