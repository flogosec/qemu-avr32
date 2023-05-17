/*
 * QEMU AVR32 TWIS
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
#include "qemu/module.h"
#include "hw/avr32/at32uc3_twis.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#define AT32UC_TWIS_CR      0x00
#define AT32UC_TWIS_NBYTES  0x04
#define AT32UC_TWIS_TR      0x08
#define AT32UC_TWIS_RHR     0x0C
#define AT32UC_TWIS_THR     0x10
#define AT32UC_TWIS_PECR    0x14
#define AT32UC_TWIS_SR      0x18
#define AT32UC_TWIS_IER     0x1C
#define AT32UC_TWIS_IDR     0x20
#define AT32UC_TWIS_IMR     0x24
#define AT32UC_TWIS_SCR     0x28
#define AT32UC_TWIS_PR      0x2C
#define AT32UC_TWIS_VR      0x30

#define TWIS_CR_SEN (1 << 0)
#define TWIS_CR_SWRST (1 << 7)
#define TWIS_CR_ACK (1 << 12)
#define TWIS_CR_CUP (1 << 13)
#define TWIS_CR_ADR_7 ( 0b1111111 << 16)
#define TWIS_CR_ADR_10 (0b1111111111 << 16)
#define TWIS_CR_TENBIT (1 << 26)

#define TWIS_SR_RXRDY (1 << 0)
#define TWIS_SR_TXRDY (1 << 1)
#define TWIS_SR_SEN (1 << 2)
#define TWIS_SR_TCOMP (1 << 3)
#define TWIS_SR_TRA (1 << 5)
#define TWIS_SR_NAK (1 << 8)
#define TWIS_SR_BTF (1 << 23)

#define AT32UC_TWIS_SR_URUN 1 << 6
#define AT32UC_TWIS_SR_ORUN 1 << 7

#define AT32UC_TWIS_SCR_MASK 0b111111110111000111001011




struct AT32UC3I2CSlaveClass {
    I2CSlaveClass parent_class;
};

typedef struct AT32UC3I2CSlaveClass AT32UC3I2CSlaveClass;

#define TYPE_AT32UC3_I2CSLAVE "at32uc3.i2c-slave"
OBJECT_DECLARE_TYPE(AT32UC3I2CSlaveState, AT32UC3I2CSlaveClass, AT32UC3_I2CSLAVE)


static void twis_update_irq(AT32UC3TWISState* s)
{
    qemu_set_irq(s->irq, !!(s->sr & s->imr));
    qemu_set_irq(s->pdca_rx_irq, !!(s->sr & TWIS_SR_RXRDY));
}

static void twis_complete_transfer(AT32UC3TWISState* s)
{
    s->sr &= ~TWIS_SR_BTF;
    i2c_nack(s->bus);
}


bool at32uc3_twis_pdca_transfer_complete(AT32UC3TWISState* s)
{
    return s->sr & TWIS_SR_TCOMP;
}

uint8_t at32uc3_twis_pdca_read_rhr(AT32UC3TWISState* s)
{
    uint8_t rhr = s->rhr;
    s->sr &= ~TWIS_SR_RXRDY;
    twis_complete_transfer(s);
    twis_update_irq(s);
    return rhr;
}



static void at32uc3_i2cslave_reset(DeviceState *dev)
{
}

static uint8_t at32uc3_i2cslave_rx(I2CSlave *i2c)
{
    printf("[at32uc3_i2cslave_rx] ret=0x%x\n", 0xff);


    return 0xff;
}

static int at32uc3_i2cslave_tx(I2CSlave *i2c, uint8_t data)
{
    AT32UC3I2CSlaveState* s = AT32UC3_I2CSLAVE(i2c);
    s->twis->sr |= TWIS_SR_BTF;
    if(s->twis->cr & TWIS_CR_CUP) {
        ++s->twis->nbytes;
    } else {
        --s->twis->nbytes;
    }

    s->twis->rhr = data;
    s->twis->sr |= TWIS_SR_RXRDY;

    twis_update_irq(s->twis);
    return 0;
}

static int at32uc3_i2cslave_event(I2CSlave *i2c, enum i2c_event event)
{
    AT32UC3I2CSlaveState* s = AT32UC3_I2CSLAVE(i2c);
    g_assert(s->twis);
    switch(event)
    {
        case I2C_START_SEND_ASYNC:
        case I2C_START_RECV:
        {
            break;
        }
        case I2C_START_SEND:
        {
            s->twis->sr &= TWIS_SR_TRA;
            break;
        }
        case I2C_FINISH:
        {
            s->twis->sr |= TWIS_SR_TCOMP;
            break;
        }
        case I2C_NACK: {
            s->twis->sr |= TWIS_SR_NAK;
            break;
        }
    }

    twis_update_irq(s->twis);

    return 0;
}

static void at32uc3_i2cslave_inst_init(Object *obj)
{
    AT32UC3I2CSlaveState* s = AT32UC3_I2CSLAVE(obj);

    s->twis = NULL;
}

static void at32uc3_i2cslave_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = at32uc3_i2cslave_reset;
    k->event = at32uc3_i2cslave_event;
    k->recv = at32uc3_i2cslave_rx;
    k->send = at32uc3_i2cslave_tx;
}

static void at32uc_twis_do_reset(AT32UC3TWISState* s)
{
    s->cr = 0x0;
    s->tr = 0x0;
    s->sr = 0x2;
    s->imr = 0x0;

    twis_update_irq(s);
}

static uint64_t at32uc_twis_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3TWISState* s = opaque;
    printf("[at32uc_twis_read] addr=0x%lx\n", addr);

    switch (addr) {
        case AT32UC_TWIS_CR: {
            return s->cr;
        }
        case AT32UC_TWIS_TR: {
            return s->tr;
        }
        case AT32UC_TWIS_RHR:
        {
            return at32uc3_twis_pdca_read_rhr(s);
        }
        case AT32UC_TWIS_SR:
        {
            printf("[at32uc_twis_read] AT32UC_TWIS_SR = 0x%x\n", s->sr);
            return s->sr;
        }
        case AT32UC_TWIS_IER:
        case AT32UC_TWIS_IDR:
        {
            printf("[at32uc_twis_read] AT32UC_TWIS_IE/DR, are write-only\n");
            return 0xdead;
        }
        case AT32UC_TWIS_IMR:
        {
            return s->imr;
        }
        case AT32UC_TWIS_SCR:
        {
            printf("[at32uc_twis_read] AT32UC_TWIS_SCR, are write-only\n");
            return 0xdead;
        }
        case AT32UC_TWIS_PR: return 0x00000000;
        case AT32UC_TWIS_VR: return 0x00000120;
        default: {
            printf("[at32uc_twis_read] Reading unknown register=0x%lx\n", addr);
            exit(-1);
        }
    }
}

static void at32uc_twis_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    uint32_t mask;
    AT32UC3TWISState* s = opaque;
    printf("[at32uc_twis_write] addr=0x%lx, val=0x%lx\n", addr, val64);

    switch(addr)
    {
        case AT32UC_TWIS_CR: {
            s->cr = val64;

            if(s->cr & TWIS_CR_SWRST) {
                at32uc_twis_do_reset(s);
                // This bit will always read as 0
                s->cr &= ~TWIS_CR_SWRST;
            }

            if(s->cr & TWIS_CR_TENBIT) {
                printf("[at32uc_twis_write] 10-bit addresses are not supported\n");
                exit(-1);
            }

            if(s->cr & TWIS_CR_ADR_7) {
                printf("[at32uc_twis_write] Setting slave address to 0x%x\n", (s->cr & TWIS_CR_ADR_7) >> 16);
                i2c_slave_set_address((I2CSlave*) s->i2c, (s->cr & TWIS_CR_ADR_7) >> 16);
            }

            if(s->cr & TWIS_CR_SEN) {
                s->sr |= TWIS_SR_SEN;

                // TODO
            }

            break;
        }
        case AT32UC_TWIS_TR: {
            s->tr = val64;
            break;
        }
        case AT32UC_TWIS_RHR:
        {
            printf("[at32uc_twis_write] AT32UC_TWIS_RHR is read-only!\n");
            break;
        }
        case AT32UC_TWIS_THR:
        {
            // TODO
            printf("[at32uc_twis_write] AT32UC_TWIS_THR is not implemented!\n");
            exit(-1);
            break;
        }
        case AT32UC_TWIS_PECR:
        {
            printf("[at32uc_twis_write] AT32UC_TWIS_PECR is read-only!\n");
            break;
        }
        case AT32UC_TWIS_SR:
        {
            printf("[at32uc_twis_write] AT32UC_TWIS_SR is read-only!\n");
            break;
        }
        case AT32UC_TWIS_IER: // Uses the same flags as SR
        {
            mask = val64 & AT32UC_TWIS_SCR_MASK;
            s->imr |= mask;
            twis_update_irq(s);
            break;
        }
        case AT32UC_TWIS_IDR:
        {
            mask = val64 & AT32UC_TWIS_SCR_MASK;
            s->imr &= ~mask;
            twis_update_irq(s);
            break;
        }
        case AT32UC_TWIS_IMR:
        {
            printf("[at32uc_twis_write] AT32UC_TWIS_IMR is read-only!\n");
            break;
        }
        case AT32UC_TWIS_SCR:
        {
            mask = val64 & AT32UC_TWIS_SCR_MASK;
            // Writing a zero to a bit in this register has no effect.
            // Writing a one to a bit in this register will clear the corresponding bit in SR and the corresponding interrupt request.
            s->sr &= ~mask;

            if(mask & TWIS_SR_BTF) {
                twis_complete_transfer(s);
            }

            twis_update_irq(s);
            break;
        }
        case AT32UC_TWIS_PR:
        case AT32UC_TWIS_VR:
        {
            printf("[at32uc_twis_write] AT32UC_TWIS_P/VR are read-only!\n");
            break;
        }
        default:
        {
            printf("[at32uc_twis_write] Writing unknown register 0x%lx", addr);
            exit(-1);
            break;
        }
    }
}

static const MemoryRegionOps twis_ops = {
        .read = at32uc_twis_read,
        .write = at32uc_twis_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_twis_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3TWISState *s = AT32UC3_TWIS(dev);
    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_irq(sbd, &s->pdca_rx_irq);
    sysbus_init_irq(sbd, &s->pdca_tx_irq);

    memory_region_init_io(&s->mmio, OBJECT(s), &twis_ops, s, TYPE_AT32UC3_TWIS, 0x400);
    sysbus_init_mmio(sbd, &s->mmio);

    s->i2c = AT32UC3_I2CSLAVE(i2c_slave_create_simple(s->bus, TYPE_AT32UC3_I2CSLAVE, 0x00));
    s->i2c->twis = s;
}

static void at32uc3_twis_reset(DeviceState *dev)
{
    at32uc_twis_do_reset(AT32UC3_TWIS(dev));
}

static void at32uc3_twis_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_twis_realize;
    dc->reset = at32uc3_twis_reset;
}


static const TypeInfo at32uc3_twis_types[] = {
        {
                .name           = TYPE_AT32UC3_TWIS,
                .parent         = TYPE_SYS_BUS_DEVICE,
                .instance_size  = sizeof(AT32UC3TWISState),
                .class_init     = at32uc3_twis_class_init,
        },
        {
                .name           = TYPE_AT32UC3_I2CSLAVE,
                .parent         = TYPE_I2C_SLAVE,
                .instance_size  = sizeof(AT32UC3I2CSlaveState),
                .instance_init  = at32uc3_i2cslave_inst_init,
                .class_size     = sizeof(AT32UC3I2CSlaveClass),
                .class_init     = at32uc3_i2cslave_class_init,
        }
};

DEFINE_TYPES(at32uc3_twis_types)