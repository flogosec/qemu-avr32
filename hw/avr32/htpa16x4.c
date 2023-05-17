/*
 * QEMU AVR32 HTPA16x4
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
#include "hw/i2c/i2c.h"
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "qom/object.h"


struct HTPA16X4State {
    I2CSlave parent_obj;
};

typedef struct HTPA16X4State HTPA16X4State;

struct HTPA16X4Class {
    I2CSlaveClass parent_class;
};

typedef struct MPU3300Class MPU3300Class;

#define TYPE_HTPA16X4 "htpa16x4"
OBJECT_DECLARE_TYPE(HTPA16X4State, HTPA16X4Class, HTPA16X4)


        static void htpa16x4_reset(DeviceState *dev)
{
}

static uint8_t htpa16x4_rx(I2CSlave *i2c)
{
    return 0xff;
}

static int htpa16x4_tx(I2CSlave *i2c, uint8_t data)
{

    return 0;
}

static int htpa16x4_event(I2CSlave *i2c, enum i2c_event event)
{
    return 0;
}

static void htpa16x4_inst_init(Object *obj)
{

}

static void htpa16x4_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = htpa16x4_reset;
    k->event = htpa16x4_event;
    k->recv = htpa16x4_rx;
    k->send = htpa16x4_tx;
}


static const TypeInfo htpa16x4_types[] = {
        {
                .name           = TYPE_HTPA16X4,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(HTPA16X4State),
                .instance_init  = htpa16x4_inst_init,
                .class_size     = sizeof(HTPA16X4Class),
                .class_init     = htpa16x4_class_init,
        }
};

DEFINE_TYPES(htpa16x4_types)