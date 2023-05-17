/*
 * QEMU AVR32 HTPA16x4_EEPROM
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


struct HTPA16X4EEPROMState {
    I2CSlave parent_obj;
};

typedef struct HTPA16X4EEPROMState HTPA16X4EEPROMState;

struct HTPA16X4EEPROMClass {
    I2CSlaveClass parent_class;
};

typedef struct MPU3300Class MPU3300Class;

#define TYPE_HTPA16X4EEPROM "htpa16x4.eeprom"
OBJECT_DECLARE_TYPE(HTPA16X4EEPROMState, HTPA16X4EEPROMClass, HTPA16X4EEPROM)


        static void htpa_eeprom_reset(DeviceState *dev)
{
}

static uint8_t htpa_eeprom_rx(I2CSlave *i2c)
{
    return 0xff;
}

static int htpa_eeprom_tx(I2CSlave *i2c, uint8_t data)
{

    return 0;
}

static int htpa_eeprom_event(I2CSlave *i2c, enum i2c_event event)
{
    return 0;
}

static void htpa_eeprom_inst_init(Object *obj)
{

}

static void htpa_eeprom_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = htpa_eeprom_reset;
    k->event = htpa_eeprom_event;
    k->recv = htpa_eeprom_rx;
    k->send = htpa_eeprom_tx;
}


static const TypeInfo htpa_eeprom_types[] = {
        {
                .name           = TYPE_HTPA16X4EEPROM,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(HTPA16X4EEPROMState),
                .instance_init  = htpa_eeprom_inst_init,
                .class_size     = sizeof(HTPA16X4EEPROMClass),
                .class_init     = htpa_eeprom_class_init,
        }
};

DEFINE_TYPES(htpa_eeprom_types)