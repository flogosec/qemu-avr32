/*
 * QEMU AVR32 AVR32UC
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
#ifndef HW_AVR32_AVR32UC_H
#define HW_AVR32_AVR32UC_H

#include "hw/sysbus.h"

#include "target/avr32/cpu.h"

#define TYPE_AVR32UC "avr32uc"
typedef struct AVR32UCState AVR32UCState;
OBJECT_DECLARE_SIMPLE_TYPE(AVR32UCState, AVR32UC)

struct AVR32UCState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    AVR32ACPU cpu;
};

#endif //HW_AVR32_AVR32UC_H
