/*
 * QEMU AVR32 FM33256b
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
#include "hw/ssi/ssi.h"
#include "qemu/module.h"
#include "qom/object.h"

#define FM33256B_WREN 0x06  // 0000 0110b
#define FM33256B_WRDI 0x04  // 0000 0100b
#define FM33256B_RDSR 0x05  // 0000 0101b
#define FM33256B_WRSR 0x01  // 0000 0001b
#define FM33256B_READ 0x03  // 0000 0011b
#define FM33256B_WRITE 0x02 // 0000 0010b
#define FM33256B_RDPC 0x13  // 0001 0011b
#define FM33256B_WRPC 0x12  // 0001 0010b

#define FM33256B_STATUS_WEL (1 << 1)
#define FM33256B_STATUS_BP0 (1 << 2)
#define FM33256B_STATUS_BP1 (1 << 3)

#define FM33256B_REGS_COUNT 0x1E
#define FM33256B_MAX_DATA 0x8000

enum fm33256b_state {
    STATE_IDLE,
    STATE_PARSE_ADDR_0,
    STATE_PARSE_ADDR_1,
    STATE_PARSE_REG_IDX,
    STATE_RUN_CMD
};


struct FM33256BState {
    SSIPeripheral parent_obj;

    enum fm33256b_state state;
    uint8_t reg_idx;
    uint16_t addr;
    uint8_t cmd;

    uint8_t status;

    uint8_t regs[FM33256B_REGS_COUNT];
    uint8_t data[FM33256B_MAX_DATA];
};

typedef struct FM33256BState FM33256BState;


struct FM33256BClass {
    SSIPeripheralClass parent_class;
};

typedef struct FM33256BClass FM33256BClass;


#define TYPE_FM33256B "fm33256b"
OBJECT_DECLARE_TYPE(FM33256BState, FM33256BClass, FM33256B)

static void fm33256b_update_addr(FM33256BState* s)
{
    s->addr = (s->addr + 1) % FM33256B_MAX_DATA;
}

static void fm33256b_update_reg_idx(FM33256BState* s)
{
    s->reg_idx = (s->reg_idx + 1) % FM33256B_REGS_COUNT;
}

static void fm33256b_reset(DeviceState *dev)
{
    FM33256BState* s = FM33256B(dev);

    s->status = 0;
    s->state = STATE_IDLE;
    s->reg_idx = 0;
    s->addr = 0;
    s->cmd = 0;

    memset(s->data, 0, FM33256B_MAX_DATA);

    memset(s->regs, 0, FM33256B_REGS_COUNT);

    // Some register have non zero default values
    s->regs[0x00] = 0x80;
    s->regs[0x0D] = 0x01;
    s->regs[0x18] = 0x40;
    s->regs[0x19] = 0x80;
    s->regs[0x1a] = 0x80;
    s->regs[0x1b] = 0x80;
    s->regs[0x1c] = 0x81;
    s->regs[0x1d] = 0x81;

}

static uint32_t fm33256b_transfer8(SSIPeripheral *ss, uint32_t tx)
{
    uint32_t ret = 0;
    FM33256BState* s = FM33256B(ss);

    switch(s->state) {
        case STATE_PARSE_ADDR_0: {
            s->addr = (tx & 0xff) << 8;
            s->state = STATE_PARSE_ADDR_1;
            break;
        }
        case STATE_PARSE_ADDR_1: {
            s->addr |= (tx & 0xff);
            s->state = STATE_RUN_CMD;
            break;
        }
        case STATE_PARSE_REG_IDX: {
            s->reg_idx = tx & 0xff;
            s->state = STATE_RUN_CMD;
            break;
        }
        case STATE_RUN_CMD: {
            switch(s->cmd) {
                case FM33256B_WREN: {
                    s->status |= FM33256B_STATUS_WEL;
                    s->status = STATE_IDLE;
                    break;
                }
                case FM33256B_WRDI: {
                    s->status &= ~FM33256B_STATUS_WEL;
                    s->status = STATE_IDLE;
                    break;
                }
                case FM33256B_RDSR: {
                    ret = s->status;
                    break;
                }
                case FM33256B_WRSR: {
                    if(s->state & FM33256B_STATUS_WEL) {
                        if(tx & FM33256B_STATUS_BP0) {
                            s->state |= FM33256B_STATUS_BP0;
                        } else {
                            s->state &= ~FM33256B_STATUS_BP0;
                        }

                        if(tx & FM33256B_STATUS_BP1) {
                            s->state |= FM33256B_STATUS_BP1;
                        } else {
                            s->state &= ~FM33256B_STATUS_BP1;
                        }

                        s->state &= ~FM33256B_STATUS_WEL;
                    }
                    break;
                }
                case FM33256B_WRITE: {
                    if(s->state & FM33256B_STATUS_WEL) {
                        s->data[s->addr] = tx & 0xff;
                        fm33256b_update_addr(s);
                    }
                    break;
                }
                case FM33256B_READ: {
                    ret = s->data[s->addr];
                    printf("[fm33256b_transfer8] FM33256B_READ data[0x%x]=0x%x\n", s->addr, ret);

                    fm33256b_update_addr(s);
                    break;
                }
                case FM33256B_RDPC: {
                    ret = s->regs[s->reg_idx];
                    fm33256b_update_reg_idx(s);
                    break;
                }
                case FM33256B_WRPC: {
                    if(s->state & FM33256B_STATUS_WEL) {
                        s->regs[s->reg_idx] = tx & 0xff;
                        fm33256b_update_reg_idx(s);
                    }
                    break;
                }
                default: {
                    printf("[fm33256b_transfer8] Executing unknown/unimplemented cmd=0x%x\n", s->cmd);
                    exit(-1);
                }
            }
            break;
        }
        case STATE_IDLE: // Parse Command
        {
            s->cmd = tx;

            switch(s->cmd) {
                case FM33256B_WREN:
                case FM33256B_WRDI:
                case FM33256B_RDSR: {
                    s->state = STATE_RUN_CMD;
                    break;
                }
                case FM33256B_WRITE:
                case FM33256B_READ: {
                    s->state = STATE_PARSE_ADDR_0;
                    break;
                }
                case FM33256B_RDPC:
                case FM33256B_WRPC: {
                    s->state = STATE_PARSE_REG_IDX;
                    break;
                }
                default: {
                    printf("[fm33256b_transfer8] Executing unknown/unimplemented cmd=0x%x\n", s->cmd);
                    exit(-1);
                }
            }
            break;
        }
    }

    return ret;
}

static int fm33256b_cs(SSIPeripheral *ss, bool select)
{
    printf("[fm33256b_cs] select=%d\n", select);
    FM33256BState* s = FM33256B(ss);

    if(select) {
        s->state = STATE_IDLE;
    }

    return 0;
}

static void fm33256b_realize(SSIPeripheral *ss, Error **errp)
{

}

static void fm33256b_inst_init(Object *obj)
{

}

static void fm33256b_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSIPeripheralClass *k = SSI_PERIPHERAL_CLASS(klass);
//    FM33256BClass *mc = FM33256B_CLASS(klass);

    k->realize = fm33256b_realize;
    k->transfer = fm33256b_transfer8;
    k->set_cs = fm33256b_cs;
    k->cs_polarity = SSI_CS_LOW;
//    dc->vmsd = &vmstate_m25p80;
//    device_class_set_props(dc, m25p80_properties);
    dc->reset = fm33256b_reset;
//    mc->pi = data;
}


static const TypeInfo fm33256b_types[] = {
        {
                .name           = TYPE_FM33256B,
                .parent         = TYPE_SSI_PERIPHERAL,
                .instance_size  = sizeof(FM33256BState),
                .instance_init  = fm33256b_inst_init,
                .class_size     = sizeof(FM33256BClass),
                .class_init     = fm33256b_class_init,
        }
};

DEFINE_TYPES(fm33256b_types)