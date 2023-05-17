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

#ifndef QEMU_AVR32_AVR32_PDCA_H
#define QEMU_AVR32_AVR32_PDCA_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/irq.h"

#define AT32UC_PDCA_PID_TWIM0_RX 6
#define AT32UC_PDCA_PID_TWIM1_RX 7
#define AT32UC_PDCA_PID_TWIS0_RX 8
#define AT32UC_PDCA_PID_TWIS1_RX 9
#define AT32UC_PDCA_PID_TWIM0_TX 17
#define AT32UC_PDCA_PID_TWIM1_TX 18
#define AT32UC_PDCA_PID_TWIM2_RX 32
#define AT32UC_PDCA_PID_TWIS2_RX 33
#define AT32UC_PDCA_PID_TWIM2_TX 35

#define AT32UC_PDCA_PID_COUNT 53

#define TYPE_AT32UC3_PDCA "at32uc3.pdca"
OBJECT_DECLARE_SIMPLE_TYPE(AT32UC3PDCAState, AT32UC3_PDCA)

#define AT32UC3PDCA_MAX_NR_CHANNELS 32

struct AT32UC3PDCAChannel {
    qemu_irq* irq;

    uint32_t mar; // Memory Address Register
    uint8_t pid;  // Peripheral Identifier in Peripheral Select Register
    uint16_t tcv; //  Transfer Counter Value in Transfer Counter Register
    uint32_t marv; // Memory Address Reload Value in Memory Address Reload Register
    uint16_t tcrv; // Transfer Counter Reload Value in Transfer Counter Reload Register

    uint32_t mr; // Mode register
    uint8_t ten;

    uint32_t imr;
    uint32_t isr;

    // Only relevant when transfer is active
    uint8_t* mar_buf;
};

typedef struct AT32UC3PDCAChannel AT32UC3PDCAChannel;

struct AT32UC3PDCAState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq irq;
    int irqline;

    MemoryRegion* ram;

    AT32UC3PDCAChannel channels[AT32UC3PDCA_MAX_NR_CHANNELS];

    DeviceState *device_states[AT32UC_PDCA_PID_COUNT];
    AT32UC3PDCAChannel* active_channels[AT32UC_PDCA_PID_COUNT];
};

AT32UC3PDCAChannel* at32uc3_pdca_is_channel_setup(AT32UC3PDCAState* s, int pdca_pid);
int at32uc3_pdca_twim_transfer(AT32UC3PDCAState* s, AT32UC3PDCAChannel* ch, I2CBus* bus);

#endif //QEMU_AVR32_AVR32_PDCA_H
