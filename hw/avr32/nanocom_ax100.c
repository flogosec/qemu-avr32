/*
 * OPSSAT Nanocom AX100
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
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "nanocom_ax100.h"
#include "opssat-simagent.h"

typedef enum __attribute__ ((packed)) rparam_action_e {
    RPARAM_GET = 0x00,
    RPARAM_REPLY = 0x55,
    RPARAM_SET = 0xFF,
    RPARAM_SET_TO_FILE = 0xEE,
    RPARAM_TABLE = 0x44,
    RPARAM_COPY = 0x77,
    RPARAM_LOAD = 0x88,
    RPARAM_SAVE = 0x99,
    RPARAM_CLEAR = 0xAA,
} rparam_action;

struct rparam_csp_packet
{
    uint8_t unknown[4];

    // From rparam lin:
    rparam_action action;
    uint8_t mem;											//! Memory area to work on (0 = RUNNING, 1 = SCRATCH)
    uint16_t length;										//! Length of the payload in bytes
    uint16_t checksum;										//! Fletcher's checksum
    uint16_t seq;
    uint16_t total;
};

static void nanocom_ax100_reset(DeviceState *dev)
{
    NanoComAX100State* s = NANOCOM_AX100(dev);

    memset(s->packet_buf, 0, NANOCOMAX100_MAX_PACKET_SIZE);
    s->packet_buf_idx = 0;
}

static uint8_t nanocom_ax100_rx(I2CSlave *i2c)
{
//    printf("[nanocom_ax100_rx] ret=0x%x\n", 0xff);
    return 0xff;
}

static int nanocom_ax100_tx(I2CSlave *i2c, uint8_t data)
{
    NanoComAX100State* s = NANOCOM_AX100(i2c);
//    printf("[nanocom_ax100_tx] data=0x%x\n", data);

    s->packet_buf[s->packet_buf_idx] = data;
    ++s->packet_buf_idx;

    return 0;
}

static int nanocom_ax100_event(I2CSlave *i2c, enum i2c_event event)
{
    NanoComAX100State* s = NANOCOM_AX100(i2c);
//    printf("[nanocom_ax100_event] event=%d\n", event);

    switch(event) {
        case I2C_START_RECV:
        case I2C_START_SEND:
        {
            printf("[nanocom_ax100_event] LOCKING\n");
            qemu_mutex_lock(&s->trx_lock);
            s->state = STATE_RECORDING_PACKET;
            s->packet_buf_idx = 0;
            break;
        }
        case I2C_FINISH:
        {
            opssat_simagent_nancom_recv_pkt(s->simagent, s->packet_buf, s->packet_buf_idx);
            printf("[nanocom_ax100_event] UNLOCKING\n");
            qemu_mutex_unlock(&s->trx_lock);
            break;
        }
        case I2C_NACK: /* Masker NACKed a receive byte.  */
        {
            break;
        }
        case I2C_START_SEND_ASYNC:
        {
            //TODO
            break;
        }
    }

    return 0;
}

#include "qemu/main-loop.h"

void nanocom_ax100_send_packet(NanoComAX100State* s, char* packet, uint32_t size)
{
    printf("[nanocom_ax100_send_packet] LOCKING...\n");

    qemu_mutex_lock(&s->trx_lock);

    printf("[nanocom_ax100_send_packet] LOCKING DONE\n");

//    printf("[nanocom_ax100_send_packet] START TRANSFER\n");
    if (i2c_start_send(s->bus, 0x1)) {
        printf("[nanocom_ax100_send_packet] Sending done...\n");
        qemu_mutex_unlock(&s->trx_lock);
        return ;
    }

    printf("[nanocom_ax100_send_packet] Emitting Packet ... \n");
    while(size--) {
//        printf("[nanocom_ax100_send_packet] send=0x%x\n", *packet);
        i2c_send(s->bus, *packet++);

        qemu_mutex_unlock_iothread();

        usleep(5 * 1000);

        qemu_mutex_lock_iothread();
    }

//    printf("[nanocom_ax100_send_packet] END TRANSFER\n");
    i2c_end_transfer(s->bus);

    printf("[nanocom_ax100_send_packet] UNLOCKING...\n");
    qemu_mutex_unlock(&s->trx_lock);
}

static void nanocom_ax100_inst_init(Object *obj)
{
    NanoComAX100State* s = NANOCOM_AX100(obj);

    printf("[nanocom_ax100_inst_init] Init lock\n");
    qemu_mutex_init(&s->trx_lock);
    s->simagent = NULL;
}

static void nanocom_ax100_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = nanocom_ax100_reset;
    k->event = nanocom_ax100_event;
    k->recv = nanocom_ax100_rx;
    k->send = nanocom_ax100_tx;
}


static const TypeInfo nanocom_ax100_types[] = {
        {
                .name           = TYPE_NANOCOM_AX100,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(NanoComAX100State),
                .instance_init  = nanocom_ax100_inst_init,
                .class_size     = sizeof(NanoComAX100Class),
                .class_init     = nanocom_ax100_class_init,
        }
};

DEFINE_TYPES(nanocom_ax100_types)