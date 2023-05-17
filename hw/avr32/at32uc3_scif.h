/*
 * QEMU AVR32 INTC
 *
 * Copyright (c) 2023, Florian Göhler
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
#ifndef QEMU_AVR32_AVR32_SCIF_H
#define QEMU_AVR32_AVR32_SCIF_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/irq.h"


#define TYPE_AT32UC3_SCIF "at32uc3.scif"
OBJECT_DECLARE_SIMPLE_TYPE(AT32UC3SCIFState, AT32UC3_SCIF)

struct AT32UC3SCIFState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    qemu_irq irq;
    int irqline;

    uint8_t num_cs;
    qemu_irq *cs_lines;

};

#endif //QEMU_AVR32_AVR32_SCIF_H
