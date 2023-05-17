/*
 * QEMU AVR32 INTC
 *
 * Copyright (c) 2023, Florian Göhler, Johannes Willbold
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

#ifndef QEMU_AVR32_AVR32_INTC_H
#define QEMU_AVR32_AVR32_INTC_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/irq.h"
#include "target/avr32/cpu.h"

#define AT32UC3_INTC_IPR_INTLEVEL 0b11 << 30
#define AT32UC3_INTC_IPR_AUTOVECTOR ((1 << 14) - 1)

#define TYPE_AT32UC3_INTC "at32uc3.intc"
OBJECT_DECLARE_SIMPLE_TYPE(AT32UC3INTCState, AT32UC3_INTC)

struct AT32UC3INTCState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq irq;
    AVR32ACPU* cpu;

    uint32_t priority_regs[64];
    uint32_t request_regs[64];
    uint8_t cause[4];

    uint64_t grp_req_lines;
    uint64_t val_req_lines;
};

uint32_t avr32_intc_get_pending_intr(AT32UC3INTCState* intc);

#endif //QEMU_AVR32_AVR32_INTC_H
