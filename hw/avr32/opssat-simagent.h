/*
 * OPSSAT Simulation Agent
 *
 * Copyright (c) 2023 Florian Göhler, Johannes Willbold
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

#ifndef QEMU_AVR32_OPSSAT_SIMAGENT_H
#define QEMU_AVR32_OPSSAT_SIMAGENT_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "nanocom_ax100.h"

struct OpsSatSimAgentState {
    DeviceState parent_obj;

    QemuThread sim_thread;

    NanoComAX100State* nanocom;
};

typedef struct OpsSatSimAgentState OpsSatSimAgentState;

struct OpsSatSimAgentClass {
    DeviceClass parent_class;
};

typedef struct OpsSatSimAgentClass OpsSatSimAgentClass;

#define TYPE_OPSSAT_SIMAGENT "opssat.sigmagent"
OBJECT_DECLARE_TYPE(OpsSatSimAgentState, OpsSatSimAgentClass, OPSSAT_SIMAGENT)

void opssat_simagent_nancom_recv_pkt(OpsSatSimAgentState* s, uint8_t* buf, uint32_t size);

#endif //QEMU_AVR32_OPSSAT_SIMAGENT_H
