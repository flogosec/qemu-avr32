/*
 * QEMU AVR32 Nanomind RM3100
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

#define RM3100_CCX_0 0x04
#define RM3100_CCX_1 0x05
#define RM3100_CCY_0 0x06
#define RM3100_CCY_1 0x07
#define RM3100_CCZ_0 0x08
#define RM3100_CCZ_1 0x09
#define RM3100_TMRC 0x0B

#define RM3100_MX0 0x24
#define RM3100_MX1 0x25
#define RM3100_MX2 0x26
#define RM3100_MY0 0x27
#define RM3100_MY1 0x28
#define RM3100_MY2 0x29
#define RM3100_MZ0 0x2a
#define RM3100_MZ1 0x2b
#define RM3100_MZ2 0x2c

#define RM3100_STATUS 0x34
#define RM3100_HSHAKE 0x35

#define RM3100_REG_COUNT 0x37


struct RM3100State {
    I2CSlave parent_obj;

    int current_reg_idx;

    uint8_t regs[RM3100_REG_COUNT];
};

typedef struct RM3100State RM3100State;

struct RM3100Class {
    I2CSlaveClass parent_class;
};

typedef struct RM3100Class RM3100Class;

#define TYPE_RM3100 "rm3100"
OBJECT_DECLARE_TYPE(RM3100State, RM3100Class, RM3100)


static void rm3100_reset(DeviceState *dev)
{
    RM3100State* s = RM3100(dev);

    memset(s->regs, 0, RM3100_REG_COUNT);

    // Non-zero default values
    s->regs[RM3100_CCX_1]  = 0xc8;
    s->regs[RM3100_CCY_1]  = 0xc8;
    s->regs[RM3100_CCZ_1]  = 0xc8;
    s->regs[RM3100_TMRC]   = 0x96;
    s->regs[RM3100_HSHAKE] = 0x1B;

    // Not by default like that:
    s->regs[RM3100_STATUS] = 0x80; // We always have a measurement ready

    s->current_reg_idx = -1;
}

static uint8_t rm3100_rx(I2CSlave *i2c)
{
    RM3100State* s = RM3100(i2c);
    uint8_t val;

    if (s->current_reg_idx >= RM3100_REG_COUNT) {
        printf("[rm3100_rx] Out-of-bounds read at idx=%d\n", s->current_reg_idx );
    } else if(s->current_reg_idx <= -1) {
        printf("[rm3100_rx] reading without initialized register index\n");
    } else {
        if(s->current_reg_idx == RM3100_CCX_0) {
            printf("[rm3100_rx] Reading Cyclic Counts\n");
        } else if(s->current_reg_idx == RM3100_MX0) {
            printf("[rm3100_rx] Reading Measurements\n");
        } else if(s->current_reg_idx == RM3100_STATUS) {
        }

        val = s->regs[s->current_reg_idx];
        ++s->current_reg_idx;
        return val;
    }

    return 0xff;
}

static int rm3100_tx(I2CSlave *i2c, uint8_t data)
{
    RM3100State* s = RM3100(i2c);

    if(s->current_reg_idx <= -1) {
        s->current_reg_idx = data;
    } else if (s->current_reg_idx >= RM3100_REG_COUNT) {
        printf("[rm3100_tx] Out-of-bounds write at idx=%d\n", s->current_reg_idx );
    } else {

        // Check if the register is writable
        if((s->current_reg_idx >= 0x04 && s->current_reg_idx <= 0x09) ||
            (s->current_reg_idx == 0x33) || (s->current_reg_idx == 0x35)) {
            s->regs[s->current_reg_idx] = data;
        }
        ++s->current_reg_idx;
    }

    return 0;
}

static int rm3100_event(I2CSlave *i2c, enum i2c_event event)
{
    RM3100State* s = RM3100(i2c);

    switch(event) {
        case I2C_START_RECV:
        case I2C_START_SEND_ASYNC:
        case I2C_START_SEND:
        {
            break;
        }
        case I2C_FINISH:
        {
            s->current_reg_idx = -1;
            break;
        }
        case I2C_NACK: /* Masker NACKed a receive byte.  */
        {
            break;
        }
    }
    return 0;
}

static void rm3100_inst_init(Object *obj)
{

}

static void rm3100_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = rm3100_reset;
    k->event = rm3100_event;
    k->recv = rm3100_rx;
    k->send = rm3100_tx;
}


static const TypeInfo rm3100_types[] = {
        {
                .name           = TYPE_RM3100,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(RM3100State),
                .instance_init  = rm3100_inst_init,
                .class_size     = sizeof(RM3100Class),
                .class_init     = rm3100_class_init,
        }
};

DEFINE_TYPES(rm3100_types)