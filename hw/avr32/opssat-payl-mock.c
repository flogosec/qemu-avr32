/*
 * OPSSAT Paylaod Mock
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

#include "qemu/osdep.h"
#include "hw/i2c/i2c.h"
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "qom/object.h"


struct OpssatPaylMockState {
    I2CSlave parent_obj;
};

typedef struct OpssatPaylMockState OpssatPaylMockState;

struct OpssatPaylMockClass {
    I2CSlaveClass parent_class;
};

typedef struct OpssatPaylMockClass OpssatPaylMockClass;

#define TYPE_OPSSAT_PAYL_MOCK "opssat-payl-mock"
OBJECT_DECLARE_TYPE(OpssatPaylMockState, OpssatPaylMockClass, OPSSAT_PAYL_MOCK)


static void opssat_payl_reset(DeviceState *dev)
{
//    OpssatPaylMockState* s = OPSSAT_PAYL_MOCK(dev);
}

static uint8_t opssat_payl_rx(I2CSlave *i2c)
{
//    printf("[opssat_payl_rx] ret=0x%x\n", 0xff);
    return 0xff;
}

static int opssat_payl_tx(I2CSlave *i2c, uint8_t data)
{
//    printf("[opssat_payl_tx] data=0x%x\n", data);

    return 0;
}

static int opssat_payl_event(I2CSlave *i2c, enum i2c_event event)
{
//    printf("[opssat_payl_event] event=%d\n", event);

    return 0;
}

static void opssat_payl_inst_init(Object *obj)
{

}

static void opssat_payl_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = opssat_payl_reset;
    k->event = opssat_payl_event;
    k->recv = opssat_payl_rx;
    k->send = opssat_payl_tx;
}


static const TypeInfo opssat_payl_types[] = {
        {
                .name           = TYPE_OPSSAT_PAYL_MOCK,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(OpssatPaylMockState),
                .instance_init  = opssat_payl_inst_init,
                .class_size     = sizeof(OpssatPaylMockClass),
                .class_init     = opssat_payl_class_init,
        }
};

DEFINE_TYPES(opssat_payl_types)