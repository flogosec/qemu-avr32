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
    //TODO: Processor specific
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
