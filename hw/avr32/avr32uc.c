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
#include "qemu/osdep.h"
#include "avr32uc.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/avr32/at32uc3_intc.h"

typedef struct AVR32UCClass AVR32UCClass;
struct AVR32UCClass {

};

static void avr32uc_inst_init(Object *obj)
{
    AVR32UCState *s = AVR32UC(obj);
    object_initialize_child(obj, "cpu", &s->cpu, AVR32A_CPU_TYPE_NAME("AT32UC3C"));
}

static void avr32uc_realize(DeviceState* dev, Error** errp)
{
    AVR32UCState *s = AVR32UC(dev);

    if (!qdev_realize(DEVICE(&s->cpu), NULL, errp)) {
        return;
    }
    object_property_set_bool(OBJECT(&s->cpu), "realized", true, &error_abort);
}

static void avr32uc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = avr32uc_realize;
}

static const TypeInfo av32uc_types[] = {
        {
                .name           = TYPE_AVR32UC,
                .parent         = TYPE_SYS_BUS_DEVICE,
                .instance_size  = sizeof(AVR32UCState),
                .instance_init  = avr32uc_inst_init,
                .class_size     = sizeof(AVR32UCClass),
                .class_init     = avr32uc_class_init,
        }
};

DEFINE_TYPES(av32uc_types)