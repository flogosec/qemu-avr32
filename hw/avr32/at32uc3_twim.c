/*
 * QEMU AVR32 TWIM
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
#include "hw/avr32/at32uc3_twim.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#include "qemu/log.h"

#define AT32UC_TWIM_CR      0x00
#define AT32UC_TWIM_CWGR    0x04
#define AT32UC_TWIM_SMBTR   0x08
#define AT32UC_TWIM_CMDR    0x0C
#define AT32UC_TWIM_NCMDR   0x10
#define AT32UC_TWIM_RHR     0x14
#define AT32UC_TWIM_THR     0x18
#define AT32UC_TWIM_SR      0x1C
#define AT32UC_TWIM_IER     0x20
#define AT32UC_TWIM_IDR     0x24
#define AT32UC_TWIM_IMR     0x28
#define AT32UC_TWIM_SCR     0x2C

#define AT32UC_TWIM_CR_MEN      1 << 0
#define AT32UC_TWIM_CR_SWRST    1 << 7

#define AT32UC_TWIM_SR_MENB 1 << 16
#define AT32UC_TWIM_SR_IDLE 1 << 4  // Master Interface is Idle

#define AT32UC_TWIM_CMDR_READ 1 << 0
#define AT32UC_TWIM_CMDR_SADR  0b1111111111 << 1
#define AT32UC_TWIM_CMDR_START 1 << 13
#define AT32UC_TWIM_CMDR_STOP  1 << 14
#define AT32UC_TWIM_CMDR_VALID 1 << 15
#define AT32UC_TWIM_CMDR_NBYTES 0b11111111 << 1

#define AT32UC_TWIM_SR_RXRDY    1 << 0
#define AT32UC_TWIM_SR_TXRDY    1 << 1
#define AT32UC_TWIM_SR_CRDY     1 << 2
#define AT32UC_TWIM_SR_CCOMP     1 << 3
#define AT32UC_TWIM_SR_IDLE     1 << 4
#define AT32UC_TWIM_SR_ANAK     1 << 8
#define AT32UC_TWIM_SR_DNAK     1 << 9
#define AT32UC_TWIM_SR_ARBLST   1 << 10
#define AT32UC_TWIM_SR_SMBALERT 1 << 11
#define AT32UC_TWIM_SR_MENB     1 << 16

#define TWIM_SR_INTR_MASK 0b0111111100111111
#define AT32UC_TWIM_SCR_MASK 0b0111111100001000


static void at32uc3_twim_do_reset(AT32UC3TWIMState* s)
{
    s->cr = 0x0;
    s->cwgr = 0x0;
    s->smbtr = 0x0;
    s->cmdr = 0x0;
    s->ncmdr = 0x0;
    s->rhr = 0xff;
    s->sr = 0x2;
    s->imr = 0x0;

    s->sr |= AT32UC_TWIM_SR_IDLE | AT32UC_TWIM_SR_CRDY;
}

static bool at32uc3_twim_is_enabled(AT32UC3TWIMState* s)
{
    return s->cr & AT32UC_TWIM_CR_MEN;
}

static void twim_update_irq(AT32UC3TWIMState* s)
{
    if(s->sr & TWIM_SR_INTR_MASK & s->imr) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static void twim_update_crdy(AT32UC3TWIMState* s)
{
    if(s->ncmdr & AT32UC_TWIM_CMDR_VALID && s->cmdr & AT32UC_TWIM_CMDR_VALID) {
        s->sr &= ~AT32UC_TWIM_SR_CRDY;

    } else {
        s->sr |= AT32UC_TWIM_SR_CRDY;
    }
}

static void twim_maybe_move_ncmdr_to_cmdr(AT32UC3TWIMState* s)
{
    if((s->cmdr & AT32UC_TWIM_CMDR_VALID) == 0 && s->ncmdr & AT32UC_TWIM_CMDR_VALID) {
        s->cmdr = s->ncmdr;
        s->ncmdr = 0;
        twim_update_crdy(s);
    }
}

static void twim_maybe_execute_cmdr(AT32UC3TWIMState* s)
{
    uint32_t sadr = 0;
    int pdca_pid;

    if(s->cmdr & AT32UC_TWIM_CMDR_SADR) {
        sadr = (s->cmdr & AT32UC_TWIM_CMDR_SADR) >> 1;
    }

    if(s->cmdr & AT32UC_TWIM_CMDR_VALID) {
        s->sr &= ~AT32UC_TWIM_SR_IDLE;

        if(s->cmdr & AT32UC_TWIM_CMDR_START) {
            if (i2c_start_transfer(s->bus, sadr, s->cmdr & AT32UC_TWIM_CMDR_READ)) {
                printf("[at32uc_twim_write] Failed to start a transfer, unknown SADR=%d?\n", sadr);
                exit(-1);
            } else {
            }
        }

        if(s->cmdr & AT32UC_TWIM_CMDR_READ) {
            pdca_pid = s->pdca_recv_pid;
        } else {
            pdca_pid = s->pdca_send_pid;
        }

        AT32UC3PDCAChannel* pdca_ch = at32uc3_pdca_is_channel_setup(s->pdca, pdca_pid);

        if(pdca_ch) {
            if(at32uc3_pdca_twim_transfer(s->pdca, pdca_ch, s->bus)) {
                s->sr |= AT32UC_TWIM_SR_CCOMP;
            } else {
                // TODO: Some error occurred
            }

            if(s->cmdr & AT32UC_TWIM_CMDR_STOP) {
                i2c_end_transfer(s->bus);
            }

            s->cmdr &= ~AT32UC_TWIM_CMDR_VALID;
            twim_update_irq(s);

            twim_maybe_move_ncmdr_to_cmdr(s);
            twim_update_irq(s);

            twim_maybe_execute_cmdr(s);

            twim_update_crdy(s);

            s->sr |= AT32UC_TWIM_SR_IDLE;
        } else {
            if(s->cmdr & AT32UC_TWIM_CMDR_READ) {
                // TODO
                printf("[at32uc_twim_write] Non-PDCA receiving is not implemented!\n");
                exit(-1);
            } else {
                s->sr |= AT32UC_TWIM_SR_TXRDY;
            }
        }

    }

    twim_update_irq(s);
}

static uint64_t at32uc_twim_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3TWIMState* s = opaque;
    switch (addr) {
        case AT32UC_TWIM_CR:
        {
            printf("[at32uc_twim_write] AT32UC_TWIM_CR is write-only\n");
            return 0xdead;
        }
        case AT32UC_TWIM_CWGR:
        {
            return s->cwgr;
        }
        case AT32UC_TWIM_SMBTR:
        {
            return s->smbtr;
        }
        case AT32UC_TWIM_CMDR:
        {
            return s->cmdr;
        }
        case AT32UC_TWIM_NCMDR:
        {
            return s->ncmdr;
        }
        case AT32UC_TWIM_RHR:
        {
            s->sr &= ~AT32UC_TWIM_SR_RXRDY;
            twim_update_irq(s);

            return s->rhr;
        }
        case AT32UC_TWIM_SR:
        {
            return s->sr;
        }
        case AT32UC_TWIM_IER:
        case AT32UC_TWIM_IDR:
        {
            printf("[at32uc_twim_read] AT32UC_TWIM_IE/DR, are write-only\n");
            return 0xdead;
        }
        case AT32UC_TWIM_IMR:
        {
            return s->imr;
        }
        case AT32UC_TWIM_SCR:
        {
            printf("[at32uc_twim_read] AT32UC_TWIM_SCR is write-only\n");
            return 0xdead;
        }
        default:
        {
            qemu_log_mask(LOG_GUEST_ERROR, "[at32uc_twim_read] reading addr=0x%lx is not implemented\n", addr);
            return 0xdead;
        }
    }
}

static void at32uc_twim_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    uint32_t mask;
    AT32UC3TWIMState* s = opaque;

    switch(addr)
    {
        case AT32UC_TWIM_CR:
        {
            s->cr = val64 & 0xffffffff;

            if(s->cr & AT32UC_TWIM_CR_MEN) {
                // Enable master interface
                s->sr |= AT32UC_TWIM_SR_MENB;
            }

            if(at32uc3_twim_is_enabled(s) && s->cr & AT32UC_TWIM_CR_SWRST) {
                printf("[at32uc_twim_write] AT32UC_TWIM_CR_SWRST \n");
                at32uc3_twim_do_reset(s);
            }

            break;
        }
        case AT32UC_TWIM_CWGR:
        {
            s->cwgr = val64;
            break;
        }
        case AT32UC_TWIM_SMBTR:
        {
            s->smbtr = val64;
            break;
        }
        case AT32UC_TWIM_CMDR:
        {
            s->cmdr = val64;
//            printf("[at32uc_twim_write] AT32UC_TWIM_CMDR, cmdr=0x%x\n", s->cmdr);

            twim_maybe_move_ncmdr_to_cmdr(s);
            twim_update_crdy(s);
            twim_update_irq(s);

            if(at32uc3_twim_is_enabled(s)) {
                twim_maybe_execute_cmdr(s);
            }
            break;
        }
        case AT32UC_TWIM_NCMDR:
        {
            s->ncmdr = val64;
            twim_maybe_move_ncmdr_to_cmdr(s);
            twim_update_crdy(s);
            twim_update_irq(s);

            if(at32uc3_twim_is_enabled(s)) {
                twim_maybe_execute_cmdr(s);
            }
            break;
        }
        case AT32UC_TWIM_RHR:
        {
            printf("[at32uc_twim_write] AT32UC_TWIM_RHR is read-only!\n");
            break;
        }
        case AT32UC_TWIM_THR:
        {
//            printf("[at32uc_twim_write] AT32UC_TWIM_THR\n");

            i2c_send(s->bus, val64 & 0xff);

            s->sr &= ~AT32UC_TWIM_SR_TXRDY;

            twim_update_irq(s);
            break;
        }
        case AT32UC_TWIM_IER: // Uses the same flags as SR
        {
            mask = val64 & 0xffffffff;

            // Writing a zero to a bit in this register has no effect.
            // Writing a one to a bit in this register will set the corresponding bit in IMR
            s->imr |= mask;
            twim_update_irq(s);

            break;
        }
        case AT32UC_TWIM_IDR:
        {
            mask = val64 & 0xffffffff;

            // Writing a zero to a bit in this register has no effect.
            // Writing a one to a bit in this register will clear the corresponding bit in IMR
            s->imr &= ~mask;
            twim_update_irq(s);

            break;
        }
        case AT32UC_TWIM_IMR:
        {
            printf("[at32uc_twim_write] AT32UC_TWIM_IMR is read-only!\n");
            break;
        }
        case AT32UC_TWIM_SCR:
        {
            mask = val64 & AT32UC_TWIM_SCR_MASK;

            // Writing a zero to a bit in this register has no effect.
            // Writing a one to a bit in this register will clear the corresponding bit in SR and the corresponding interrupt request.
            s->sr &= ~mask;
            twim_update_irq(s);
            break;
        }
        default:
        {
            printf("[at32uc_twim_write] Not implemented! addr=0x%lx\n", addr);
            break;
        }
    }
}

static const MemoryRegionOps twim_ops = {
        .read = at32uc_twim_read,
        .write = at32uc_twim_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_twim_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3TWIMState *s = AT32UC3_TWIM(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &twim_ops, s, TYPE_AT32UC3_TWIM, 0x100);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    s->bus = i2c_init_bus(dev, "at32uc3c.twim");
}

static void at32uc3_twim_reset(DeviceState *dev)
{
    AT32UC3TWIMState *s = AT32UC3_TWIM(dev);
    at32uc3_twim_do_reset(s);
}

static void at32uc3_twim_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_twim_realize;
    dc->reset = at32uc3_twim_reset;
    dc->desc = "AT32UC3C TWIM Controller";
}

static const TypeInfo at32uc3_twim_info = {
        .name           = TYPE_AT32UC3_TWIM,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3TWIMState),
        .class_init     = at32uc3_twim_class_init,
};

static void at32uc3_twim_register_types(void)
{
    type_register_static(&at32uc3_twim_info);
}

type_init(at32uc3_twim_register_types)