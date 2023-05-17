/*
 * QEMU AVR32 HMC5843
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

#define REG_CONFIG_A 0
#define REG_CONFIG_B 1
#define REG_MODE     2
#define REG_DATA_OUT_X_MSB 3
#define REG_DATA_OUT_X_LSB 4
#define REG_DATA_OUT_Y_MSB 5
#define REG_DATA_OUT_Y_LSB 6
#define REG_DATA_OUT_Z_MSB 7
#define REG_DATA_OUT_Z_LSB 8
#define REG_STATUS         9
#define REG_IDENT_A 10
#define REG_IDENT_B 11
#define REG_IDENT_C 12



struct HMC5843State {
    I2CSlave parent_obj;

    uint8_t conf_a;
    uint8_t conf_b;
    uint8_t mode;

    uint16_t data_out_x;
    uint16_t data_out_y;
    uint16_t data_out_z;

    int current_reg_idx;
};

typedef struct HMC5843State HMC5843State;

struct HMC5843Class {
    I2CSlaveClass parent_class;
};

typedef struct HMC5843Class HMC5843Class;

#define TYPE_HMC5843 "hmc5843"
OBJECT_DECLARE_TYPE(HMC5843State, HMC5843Class, HMC5843)


static uint8_t hmc5843_rx(I2CSlave *i2c)
{
    HMC5843State* s = HMC5843(i2c);
    int current_reg = s->current_reg_idx;

    if(s->current_reg_idx == 9) {
        s->current_reg_idx = 3;
    } else if(s->current_reg_idx >= 12) {
        s->current_reg_idx = 0;
    } else {
        ++s->current_reg_idx;
    }

    if(current_reg == REG_CONFIG_A) {
    } else if(current_reg == REG_IDENT_A) {
    } else if(current_reg == REG_DATA_OUT_X_MSB) {
        printf("[hmc5843_rx] Reading Compass\n");
    }

    switch(current_reg) {
        case REG_CONFIG_A: return s->conf_a;
        case REG_CONFIG_B: return s->conf_b;
        case REG_MODE: return s->mode;
        case REG_DATA_OUT_X_MSB: return s->data_out_x >> 8;
        case REG_DATA_OUT_X_LSB: return s->data_out_x & 0xff;
        case REG_DATA_OUT_Y_MSB: return s->data_out_y >> 8;
        case REG_DATA_OUT_Y_LSB: return s->data_out_y & 0xff;
        case REG_DATA_OUT_Z_MSB: return s->data_out_z >> 8;
        case REG_DATA_OUT_Z_LSB: return s->data_out_z & 0xff;
        case REG_STATUS: return 0b00000101;
        case REG_IDENT_A: return 'H';
        case REG_IDENT_B: return '4';
        case REG_IDENT_C: return '3';
        default: {
            printf("[hmc5843_tx] Attempting to read write-only or out-of-bounds reg[%d]\n", s->current_reg_idx);
        }
    }

    return 0xff;
}

static int hmc5843_tx(I2CSlave *i2c, uint8_t data)
{
    HMC5843State* s = HMC5843(i2c);

    if(s->current_reg_idx == -1) {
        s->current_reg_idx = data;
        return 0;
    }

    switch(s->current_reg_idx) {
        case REG_CONFIG_A: {
            printf("[hmc5843_tx] CONF_A=0x%x\n", data);
              s->conf_a = data;
            break;
        }
        case REG_CONFIG_B: {
            printf("[hmc5843_tx] CONF_B=0x%x\n", data);
            s->conf_b = data;
            break;
        }
        case REG_MODE: {
//            printf("[hmc5843_tx] MODE=0x%x\n", data);
            s->mode = data;
            break;
        }
        default: {
            printf("[hmc5843_tx] Attempting to write read-only or out-of-bounds reg[%d]=0x%x\n", s->current_reg_idx, data);
            return 1;
        }
    }

    ++s->current_reg_idx;

    return 0;
}

static int hmc5843_event(I2CSlave *i2c, enum i2c_event event)
{
    HMC5843State* s = HMC5843(i2c);
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

static void hmc5843_reset(DeviceState *dev)
{
    HMC5843State* s = HMC5843(dev);

    s->conf_a = 0;
    s->conf_b = 0;
    s->mode = 0;

    s->current_reg_idx = -1;
}

static void hmc5843_inst_init(Object *obj)
{
    printf("[hmc5843_inst_init]\n");
}

static void hmc5843_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = hmc5843_reset;
    k->event = hmc5843_event;
    k->recv = hmc5843_rx;
    k->send = hmc5843_tx;
}

static const TypeInfo hmc5843_types[] = {
        {
                .name           = TYPE_HMC5843,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(HMC5843State),
                .instance_init  = hmc5843_inst_init,
                .class_size     = sizeof(HMC5843Class),
                .class_init     = hmc5843_class_init,
        }
};

DEFINE_TYPES(hmc5843_types)