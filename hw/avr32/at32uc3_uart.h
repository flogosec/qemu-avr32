/*
 * QEMU AVR32 UART
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
#ifndef QEMU_AVR32_AVR32_UART_H
#define QEMU_AVR32_AVR32_UART_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/irq.h"


#define TYPE_AT32UC3_UART "at32uc3.uart"
OBJECT_DECLARE_SIMPLE_TYPE(AT32UC3UARTState, AT32UC3_UART)

struct AT32UC3UARTState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    qemu_irq irq;
    int irqline;

    uint8_t num_cs;
    qemu_irq *cs_lines;

};

#endif //QEMU_AVR32_AVR32_UART_H
