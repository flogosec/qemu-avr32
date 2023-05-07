/*
 * QEMU AVR CPU
 *
 * Copyright (c) 2022-2023 Florian Göhler, Johannes Willbold
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
#ifndef AVR32A_CPU_QOM_H
#define AVR32A_CPU_QOM_H

#include "hw/core/cpu.h"
#include "qom/object.h"

#define TYPE_AVR32A_CPU "avr32a-cpu"
#define TYPE_AVR32B_CPU "avr32b-cpu"

OBJECT_DECLARE_CPU_TYPE(AVR32ACPU, AVR32ACPUClass, AVR32A_CPU)

typedef struct AVR32ACPUDef AVR32ACPUDef;

/**
 *  AVR32ACPUClass:
 *  @parent_realize: The parent class' realize handler.
 *  @parent_reset: The parent class' reset handler.
 *
 *  A AVR32 CPU model.
 */
struct AVR32ACPUClass {
    /*< private >*/
    CPUClass parent_class;

    /*< public >*/
    DeviceRealize parent_realize;
    DeviceReset parent_reset;

    AVR32ACPUDef* cpu_def;
};

#endif // AVR32A_CPU_QOM_H