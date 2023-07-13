/*
 * OPSSAT Nanocom AX100
 *
 * Copyright (c) 2023 Johannes Willbold, Florian Göhler
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

#ifndef QEMU_AVR32_NANOCOM_AX100_H
#define QEMU_AVR32_NANOCOM_AX100_H

#include "qom/object.h"
#include "hw/i2c/i2c.h"

#define NANOCOMAX100_MAX_PACKET_SIZE 0x1000

struct OpsSatSimAgentState;
typedef struct OpsSatSimAgentState OpsSatSimAgentState;


enum CSPInterfaceState {
    STATE_RECORDING_PACKET
};


struct NanoComAX100State {
    I2CSlave parent_obj;

    enum CSPInterfaceState state;

    uint8_t packet_buf[NANOCOMAX100_MAX_PACKET_SIZE];
    int packet_buf_idx;

    I2CBus* bus;

    QemuMutex trx_lock;

    OpsSatSimAgentState* simagent;
};

typedef struct NanoComAX100State NanoComAX100State;

struct NanoComAX100Class {
    I2CSlaveClass parent_class;
};

typedef struct NanoComAX100Class NanoComAX100Class;

#define TYPE_NANOCOM_AX100 "nanocom.ax100"
OBJECT_DECLARE_TYPE(NanoComAX100State, NanoComAX100Class, NANOCOM_AX100)

void nanocom_ax100_send_packet(NanoComAX100State* s, char* packet, uint32_t size);

#endif //QEMU_AVR32_NANOCOM_AX100_H
