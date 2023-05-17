/*
 * QEMU AVR32 TWIS
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

#ifndef QEMU_AVR32_AVR32_TWIS0_H
#define QEMU_AVR32_AVR32_TWIS0_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/irq.h"
#include "hw/i2c/i2c.h"


#define TYPE_AT32UC3_TWIS "at32uc3.twis"
OBJECT_DECLARE_SIMPLE_TYPE(AT32UC3TWISState, AT32UC3_TWIS)

struct AT32UC3TWISState;
typedef struct AT32UC3TWISState AT32UC3TWISState;

struct AT32UC3I2CSlaveState {
    I2CSlave parent_obj;

    AT32UC3TWISState* twis;
};

typedef struct AT32UC3I2CSlaveState AT32UC3I2CSlaveState;

struct AT32UC3TWISState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    AT32UC3I2CSlaveState* i2c;
    qemu_irq irq;
    qemu_irq pdca_rx_irq;
    qemu_irq pdca_tx_irq;
    I2CBus * bus; // Bus from the TWIM port

    uint8_t rhr;

    uint32_t cr;
    uint8_t nbytes;
    uint32_t tr;
    uint32_t sr;
    uint32_t imr;
};

bool at32uc3_twis_pdca_transfer_complete(AT32UC3TWISState* s);
uint8_t at32uc3_twis_pdca_read_rhr(AT32UC3TWISState* s);

#endif //QEMU_AVR32_AVR32_TWIS0_H
