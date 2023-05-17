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

#ifndef QEMU_AVR32_AVR32_TWIM_H
#define QEMU_AVR32_AVR32_TWIM_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/irq.h"
#include "qemu/timer.h"
#include "hw/i2c/i2c.h"
#include "at32uc3_pdca.h"


#define TYPE_AT32UC3_TWIM "at32uc3.twim"
OBJECT_DECLARE_SIMPLE_TYPE(AT32UC3TWIMState, AT32UC3_TWIM)

struct AT32UC3TWIMState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    I2CBus* bus;
    qemu_irq irq;
    AT32UC3PDCAState* pdca;
    int pdca_recv_pid;
    int pdca_send_pid;

    uint32_t cr;
    uint32_t cwgr;
    uint32_t smbtr;
    uint32_t cmdr;
    uint32_t ncmdr;
    uint8_t rhr;
    uint32_t sr;
    uint32_t imr;
};

#endif //QEMU_AVR32_AVR32_TWIM_H
