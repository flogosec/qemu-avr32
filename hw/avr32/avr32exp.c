/*
 * QEMU AVR32 Example board
 *
 * Copyright (c) 2022-2023 Florian Göhler
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

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/misc/unimp.h"
#include "avr32exp.h"

struct AVR32EXPMcuClass {
    /*< private >*/
    SysBusDeviceClass parent_class;

    /*< public >*/
    const char *cpu_type;

    size_t flash_size;
};

typedef struct AVR32EXPMcuClass AVR32EXPMcuClass;

DECLARE_CLASS_CHECKERS(AVR32EXPMcuClass, AVR32EXP_MCU,
        TYPE_AVR32EXP_MCU)

// This functions sets up the device
static void avr32exp_realize(DeviceState *dev, Error **errp)
{
    printf("Realizing...\n");
    AVR32EXPMcuState *s = AVR32EXP_MCU(dev);
    const AVR32EXPMcuClass *mc = AVR32EXP_MCU_GET_CLASS(dev);

    /* CPU */
    object_initialize_child(OBJECT(dev), "cpu", &s->cpu, mc->cpu_type);
    object_property_set_bool(OBJECT(&s->cpu), "realized", true, &error_abort);

    /* Flash */
    memory_region_init_rom(&s->flash, OBJECT(dev),
                           "flash", mc->flash_size, &error_fatal);
    memory_region_add_subregion(get_system_memory(),
                                0xd0000000, &s->flash);
}

static void avr32exp_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = avr32exp_realize;
    dc->user_creatable = false;
}

static void avr32exps_class_init(ObjectClass *oc, void *data){

    AVR32EXPMcuClass* avr32exp = AVR32EXP_MCU_CLASS(oc);

    avr32exp->cpu_type = AVR32A_CPU_TYPE_NAME("AVR32EXPC");
    avr32exp->flash_size = 1024 * KiB;
}

static const TypeInfo avr32exp_mcu_types[] = {
        {
                .name           = TYPE_AVR32EXPS_MCU,
                .parent         = TYPE_AVR32EXP_MCU,
                .class_init     = avr32exps_class_init,
        }, {
                .name           = TYPE_AVR32EXP_MCU,
                .parent         = TYPE_SYS_BUS_DEVICE,
                .instance_size  = sizeof(AVR32EXPMcuState),
                .class_size     = sizeof(AVR32EXPMcuClass),
                .class_init     = avr32exp_class_init,
                .abstract       = true,
        }
};

DEFINE_TYPES(avr32exp_mcu_types)