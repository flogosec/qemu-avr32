/*
 * QEMU AVR32 NanoPower
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


struct NanoPowerP31uState {
    I2CSlave parent_obj;
};

typedef struct NanoPowerP31uState NanoPowerP31uState;

struct NanoPowerP31uClass {
    I2CSlaveClass parent_class;
};

typedef struct NanoPowerP31uClass NanoPowerP31uClass;

#define TYPE_NANOPOWER_P31U "nanopower.p31u"
OBJECT_DECLARE_TYPE(NanoPowerP31uState, NanoPowerP31uClass, NANOPOWER_P31U)


static void nanopower_p31_reset(DeviceState *dev)
{

}

static uint8_t nanopower_p31_rx(I2CSlave *i2c)
{
    printf("[nanopower_p31_rx] ret=0x%x\n", 0xff);
    return 0xff;
}

static int nanopower_p31_tx(I2CSlave *i2c, uint8_t data)
{
    printf("[nanopower_p31_tx] data=0x%x\n", data);

    return 0;
}

static int nanopower_p31_event(I2CSlave *i2c, enum i2c_event event)
{
    return 0;
}

static void nanopower_p31u_inst_init(Object *obj)
{

}

static void nanopower_p31u_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = nanopower_p31_reset;
    k->event = nanopower_p31_event;
    k->recv = nanopower_p31_rx;
    k->send = nanopower_p31_tx;
}


static const TypeInfo nanopowerp31u_types[] = {
        {
                .name           = TYPE_NANOPOWER_P31U,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(NanoPowerP31uState),
                .instance_init  = nanopower_p31u_inst_init,
                .class_size     = sizeof(NanoPowerP31uClass),
                .class_init     = nanopower_p31u_class_init,
        }
};

DEFINE_TYPES(nanopowerp31u_types)