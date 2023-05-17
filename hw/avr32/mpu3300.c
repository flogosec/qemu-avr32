/*
 * QEMU AVR32 MPU3300
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

#define REG_SELF_TEST_X 13
#define REG_SELF_TEST_Y 14
#define REG_SELF_TEST_Z 15

#define REG_GYRO_CONFIG 27

#define REG_GYRO_TEMP_H 65
#define REG_GYRO_TEMP_L 66
#define REG_GYRO_XOUT_H 67
#define REG_GYRO_XOUT_L 68
#define REG_GYRO_YOUT_H 69
#define REG_GYRO_YOUT_L 70
#define REG_GYRO_ZOUT_H 71
#define REG_GYRO_ZOUT_L 72

#define REG_WHOAMI 117

#define REG_COUNT 0x77


struct MPU3300State {
    I2CSlave parent_obj;

    uint8_t regs[REG_COUNT];

    int current_reg_idx;
};

typedef struct MPU3300State MPU3300State;

struct MPU3300Class {
    I2CSlaveClass parent_class;
};

typedef struct MPU3300Class MPU3300Class;

#define TYPE_MPU3300 "mpu3300"
OBJECT_DECLARE_TYPE(MPU3300State, MPU3300Class, MPU3300)


static uint8_t mpu3300_rx(I2CSlave *i2c)
{
    MPU3300State* s = MPU3300(i2c);
    uint8_t rx;

    if(!s->current_reg_idx) {
        printf("[mpu3300_rx] Reading without setting current registers index\n");
        return 0xff; // No idea if this is the actual response
    } else if (s->current_reg_idx >= REG_COUNT) {
        printf("[mpu3300_rx] Reading register out of bounds 0x%x > 0x%x\n", s->current_reg_idx, REG_COUNT);
        return 0xff; // No idea if this is the actual response
    } else {
        rx = s->regs[s->current_reg_idx];

        if(s->current_reg_idx == REG_GYRO_TEMP_H) {
            printf("[mpu3300_rx] Reading Temperature\n");
        } else if (s->current_reg_idx  == REG_GYRO_XOUT_H) {
            printf("[mpu3300_rx] Reading Gyro\n");
        }
        ++s->current_reg_idx;
        return rx;
    }
}

static int mpu3300_tx(I2CSlave *i2c, uint8_t data)
{
    MPU3300State* s = MPU3300(i2c);

    if(!s->current_reg_idx) {
        s->current_reg_idx = data;
    } else if(s->current_reg_idx >= REG_COUNT) {
        printf("[mpu3300_tx] Writing register out of bounds 0x%x > 0x%x\n", s->current_reg_idx, REG_COUNT);
        return 1;
    } else {
        s->regs[s->current_reg_idx] = data;
        ++s->current_reg_idx;
    }

    return 0;
}

static int mpu3300_event(I2CSlave *i2c, enum i2c_event event)
{
    MPU3300State* s = MPU3300(i2c);

    switch(event) {
        case I2C_START_RECV:
        case I2C_START_SEND_ASYNC:
        case I2C_START_SEND:
        {
            break;
        }
        case I2C_FINISH:
        {
            s->current_reg_idx = 0;
            break;
        }
        case I2C_NACK: /* Masker NACKed a receive byte.  */
        {
            break;
        }
    }
    return 0;
}

static void mpu3300_reset(DeviceState *dev)
{
    MPU3300State* s = MPU3300(dev);

    // All registers are reset to 0x00, except a few ...
    for(int i = 0; i < REG_COUNT; ++i) {
        s->regs[i] = 0x00;
    }

    // The following are set to non-zero
    s->regs[REG_SELF_TEST_X] = 0b00010101;
    s->regs[REG_SELF_TEST_Y] = 0b00010101;
    s->regs[REG_SELF_TEST_Z] = 0b00010101;

    // This has a specific value
    s->regs[REG_WHOAMI] = 0x68;

    s->current_reg_idx = 0;
}

static void mpu3300_inst_init(Object *obj)
{

}

static void mpu3300_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = mpu3300_reset;
    k->event = mpu3300_event;
    k->recv = mpu3300_rx;
    k->send = mpu3300_tx;
}

static const TypeInfo mpu3300_types[] = {
        {
                .name           = TYPE_MPU3300,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(MPU3300State),
                .instance_init  = mpu3300_inst_init,
                .class_size     = sizeof(MPU3300Class),
                .class_init     = mpu3300_class_init,
        }
};

DEFINE_TYPES(mpu3300_types)