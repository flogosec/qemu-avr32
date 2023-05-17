/*
 * QEMU AVR32 GSSB
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


struct GSSBIStageState {
    I2CSlave parent_obj;
};

typedef struct GSSBIStageState GSSBIStageState;

struct GSSBIStageClass {
    I2CSlaveClass parent_class;
};

typedef struct MPU3300Class MPU3300Class;

#define TYPE_GSSBIStage "gssb.istage"
OBJECT_DECLARE_TYPE(GSSBIStageState, GSSBIStageClass, GSSBIStage)


        static void istage_reset(DeviceState *dev)
{
}

static uint8_t istage_rx(I2CSlave *i2c)
{
    return 0xff;
}

static int istage_tx(I2CSlave *i2c, uint8_t data)
{
    return 0;
}

static int istage_event(I2CSlave *i2c, enum i2c_event event)
{
    return 0;
}

static void istage_inst_init(Object *obj)
{

}

static void istage_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = istage_reset;
    k->event = istage_event;
    k->recv = istage_rx;
    k->send = istage_tx;
}


static const TypeInfo istage_types[] = {
        {
                .name           = TYPE_GSSBIStage,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(GSSBIStageState),
                .instance_init  = istage_inst_init,
                .class_size     = sizeof(GSSBIStageClass),
                .class_init     = istage_class_init,
        }
};

DEFINE_TYPES(istage_types)