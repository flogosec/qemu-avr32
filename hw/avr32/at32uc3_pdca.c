/*
 * QEMU AVR32 PDCA
 *
 * Copyright (c) 2023, Florian Göhler
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
#include "hw/avr32/at32uc3_pdca.h"
#include "migration/vmstate.h"
#include "exec/address-spaces.h"

#include "at32uc3_twis.h"

#define PDCA_CHANNEL_REGION_SIZE 0x40

#define PDCA_CHANNEL_MAR 0x0
#define PDCA_CHANNEL_PSR 0x4
#define PDCA_CHANNEL_TCR 0x8
#define PDCA_CHANNEL_MARR 0xC
#define PDCA_CHANNEL_TCRR 0x10
#define PDCA_CHANNEL_CR 0x14
#define PDCA_CHANNEL_MR 0x18
#define PDCA_CHANNEL_SR 0x1C
#define PDCA_CHANNEL_IER 0x20
#define PDCA_CHANNEL_IDR 0x24
#define PDCA_CHANNEL_IMR 0x28
#define PDCA_CHANNEL_ISR 0x2c

#define PDCA_CHANNEL_CR_TEN 1 << 0
#define PDCA_CHANNEL_CR_TDIS 1 << 1
#define PDCA_CHANNEL_CR_ECLR 1 << 8

#define PDCA_CHANNEL_MR_SIZE 0b11 << 0
#define PDCA_CHANNEL_MR_ETRIG 1 << 2
#define PDCA_CHANNEL_MR_RING 1 << 3

#define PDCA_ISR_RCZ 1 << 0
#define PDCA_ISR_TRC 1 << 1
#define PDCA_ISR_TERR 1 << 1

#define PDCA_CHANNELS_PID_MAX 53


static void at32uc3_pdca_update_isr(AT32UC3PDCAState* s, AT32UC3PDCAChannel* ch) {
    if(ch->imr & ch->isr) {
        qemu_irq_raise(s->irq);
    }
}

static void at32uc3_pdca_reset(DeviceState *dev)
{
    AT32UC3PDCAState *s = AT32UC3_PDCA(dev);

    for(int i = 0; i < AT32UC3PDCA_MAX_NR_CHANNELS; ++i) {
        s->channels[i].mar = 0x0;
        s->channels[i].pid = 0x0;
        s->channels[i].tcv = 0x0;
        s->channels[i].marv = 0x0;
        s->channels[i].tcrv = 0x0;
        s->channels[i].mr = 0x0;
        s->channels[i].ten = 0x0;
        s->channels[i].imr = 0x0;
        s->channels[i].isr = 0x0;

        s->channels[i].mar_buf = 0x0;
    }
}

static bool pdca_channel_pid_is_recv(uint8_t pid)
{
    switch(pid)
    {
        case AT32UC_PDCA_PID_TWIM0_RX:
        case AT32UC_PDCA_PID_TWIM1_RX:
        case AT32UC_PDCA_PID_TWIM2_RX:
        {
            return true;
        }
        case AT32UC_PDCA_PID_TWIM0_TX:
        case AT32UC_PDCA_PID_TWIM1_TX:
        case AT32UC_PDCA_PID_TWIM2_TX:
        {
            return false;
        }
        default: {
            printf("[pdca_channel_pid_is_recv] unknown PDCA PID=%d", pid);
            exit(-1);
        }

    }
}

static void* at32uc3_pdca_mar_addr_to_ptr(AT32UC3PDCAState* s, AT32UC3PDCAChannel* ch) {
    return memory_region_get_ram_ptr(s->ram) + ch->mar - 0xd0000000;
}

static uint64_t at32uc_pdca_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3PDCAState* s = opaque;

    uint32_t offset = addr;
    if(addr < 0x800) {
        uint32_t channel_idx = offset / PDCA_CHANNEL_REGION_SIZE;
        AT32UC3PDCAChannel* channel = &s->channels[channel_idx];
        uint32_t channel_offset = offset % PDCA_CHANNEL_REGION_SIZE;

        switch(channel_offset) {
            case PDCA_CHANNEL_MAR:
            {
                return channel->mar;
            }
            case PDCA_CHANNEL_PSR:
            {
                return channel->pid;
            }
            case PDCA_CHANNEL_TCR:
            {
                return channel->tcv;
            }
            case PDCA_CHANNEL_MARR:
            {
                return channel->marv;
            }
            case PDCA_CHANNEL_TCRR:
            {
                return channel->tcrv;
            }
            case PDCA_CHANNEL_CR:
            {
                printf("[at32uc_pdca_read] PDCA_CHANNEL_CR is write-only\n");
                return 0xdeadbeef;
            }
            case PDCA_CHANNEL_MR:
            {
                return channel->mr;
            }
            case PDCA_CHANNEL_SR:
            {
                return channel->ten;
            }
            case PDCA_CHANNEL_IER:
            case PDCA_CHANNEL_IDR:
            {
                printf("[at32uc_pdca_read] PDCA_CHANNEL_IE/DR is write-only\n");
                return 0xdeadbeef;
            }
            case PDCA_CHANNEL_IMR:
            {
                return channel->imr;
            }
            case PDCA_CHANNEL_ISR:
            {
                // TODO
                return 0x0;
            }
            default: {
                return 0xdeadbeef;
            }
        }

    } else {
        // TODO: Monitoring stuff
        return 0xdeadbeef;
    }
}

static void at32uc_pdca_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    AT32UC3PDCAState* s = opaque;

    uint32_t offset = addr;
    uint32_t mask;

    if(addr < 0x800) {
        uint32_t channel_idx = offset / PDCA_CHANNEL_REGION_SIZE;
        AT32UC3PDCAChannel* channel = &s->channels[channel_idx];
        uint32_t channel_offset = offset % PDCA_CHANNEL_REGION_SIZE;

        switch(channel_offset) {
            case PDCA_CHANNEL_MAR: {
                channel->mar = val64;
                if(s && s->ram) {
                    channel->mar_buf = at32uc3_pdca_mar_addr_to_ptr(s, channel);
                }
                break;
            }
            case PDCA_CHANNEL_PSR: {
                channel->pid = val64;
                break;
            }
            case PDCA_CHANNEL_TCR:
            {
                channel->tcv = val64;
                if(channel->tcv) {
                    // This bit is cleared when the TCR and/or the TCRR holds a non-zero value.
                    channel->isr &= ~PDCA_ISR_TRC;
                }
                break;
            }
            case PDCA_CHANNEL_MARR:
            {
                channel->marv = val64;
                break;
            }
            case PDCA_CHANNEL_TCRR:
            {
                channel->tcrv = val64;

                if(channel->tcrv) {
                    // This bit is cleared when the TCRR holds a non-zero value.
                    channel->isr &= ~PDCA_ISR_RCZ;

                    // This bit is cleared when the TCR and/or the TCRR holds a non-zero value.
                    channel->isr &= ~PDCA_ISR_TRC;
                }

                break;
            }
            case PDCA_CHANNEL_CR:
            {
                if(val64 & PDCA_CHANNEL_CR_TEN) {
                    // Writing a zero to this bit has no effect.
                    // Writing a one to this bit will enable transfer for the DMA channel
                    channel->ten = 1;
                    s->active_channels[channel->pid] = channel;
                }

                if(val64 & PDCA_CHANNEL_CR_TDIS) {
                    // Writing a zero to this bit has no effect.
                    // Writing a one to this bit will disable transfer for the DMA channel
                    channel->ten = 0;
                    s->active_channels[channel->pid] = NULL;
                }

                if((val64 & PDCA_CHANNEL_CR_TEN) && (val64 & PDCA_CHANNEL_CR_TDIS)) {
                    printf("[at32uc_pdca_write] PDCA_CHANNEL_CR - Warning - Enabled and disabled DMA channel\n");
                }

                if(val64 & PDCA_CHANNEL_CR_ECLR) {
                    // Writing a zero to this bit has no effect.
                    // Writing a one to this bit will clear the Transfer Error bit in the Status Register (SR.TERR). Clearing the SR.TERR bit will allow the
                    // channel to transmit data. The memory address must first be set to point to a valid location
                    channel->isr &= ~PDCA_CHANNEL_TCRR;
                }
                break;
            }

            case PDCA_CHANNEL_MR:
            {
                channel->mr = val64;

                // TODO

                break;
            }
            case PDCA_CHANNEL_SR:
            {
                printf("[at32uc_pdca_write] PDCA_CHANNEL_SR is read-only\n");
                break;
            }
            case PDCA_CHANNEL_IER:
            {
                mask = val64 & 0xffffffff;
                channel->imr |= mask;
                break;
            }
            case PDCA_CHANNEL_IDR:
            {
                mask = val64 & 0xffffffff;
                channel->imr &= ~mask;
                break;
            }
            case PDCA_CHANNEL_IMR:
            {
                printf("[at32uc_pdca_write] PDCA_CHANNEL_IMR is read-only\n");
                break;
            }
            default:
            {
                printf("[at32uc_pdca_write] channel_offset=0x%x is unknown\n", channel_offset);
            }
        }
    } else {
        // TODO: Monitoring stuff
    }
}

static const MemoryRegionOps pdca_ops = {
        .read = at32uc_pdca_read,
        .write = at32uc_pdca_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

AT32UC3PDCAChannel* at32uc3_pdca_is_channel_setup(AT32UC3PDCAState* s, int pdca_pid)
{
    for(int i = 0; i < 32; ++i) {
        if(s->channels[i].pid == pdca_pid) {
            return &s->channels[i];
        }
    }

    return NULL;
}

static void pdca_update_tcv(AT32UC3PDCAState *s, AT32UC3PDCAChannel* ch)
{
    --ch->tcv;
    if(!ch->tcv && (ch->tcrv > 0)) {
        // RCZ: Reload Counter Zero
        // When TCR reaches zero, it will be reloaded with TCRV if TCRV has a positive value. If TCRV
        // is zero, no more transfers will be performed for the channel. When TCR is reloaded, the TCRR register is cleared.
        ch->tcv = ch->tcrv;
        ch->tcrv = 0;

        ch->isr |= PDCA_ISR_RCZ;
        at32uc3_pdca_update_isr(s, ch);
    }
}

int at32uc3_pdca_twim_transfer(AT32UC3PDCAState* s, AT32UC3PDCAChannel* ch, I2CBus* bus)
{
    int ret = 0, i;
    bool is_recv = pdca_channel_pid_is_recv(ch->pid);
    uint8_t* mar_buf = memory_region_get_ram_ptr(s->ram) + ch->mar - 0xd0000000;


    i = 0;
    while(ch->tcv) {
        if(is_recv) {
            mar_buf[i] = i2c_recv(bus);
        } else {
            ret = i2c_send(bus, mar_buf[i]);
        }
        if (ret) {
            // This bit is set when one or more transfer errors has occurred since reset or the last write to CR.ECLR
            ch->isr |= PDCA_ISR_TERR;
            at32uc3_pdca_update_isr(s, ch);
            break;
        }
        pdca_update_tcv(s, ch);
        ++i;
    }

    // TRC: Transfer Complete
    // This bit is cleared when the TCR and/or the TCRR holds a non-zero value.
    // This bit is set when both the TCR and the TCRR are zero.
    ch->isr |= PDCA_ISR_TRC;
    at32uc3_pdca_update_isr(s, ch);

    return 1; // !(ch->isr & PDCA_ISR_TERR)
}

static void pdca_handle_irq(void *opaque, int irq, int level)
{
    if(!level) {
        return;
    }

    AT32UC3PDCAState *s = AT32UC3_PDCA(opaque);
    AT32UC3PDCAChannel* ch = s->active_channels[irq];
    if(!ch) {
        return;
    }

    switch(irq) {
        case AT32UC_PDCA_PID_TWIS0_RX:
        case AT32UC_PDCA_PID_TWIS1_RX:
        case AT32UC_PDCA_PID_TWIS2_RX:
        {
            g_assert(s->device_states[irq]);
            AT32UC3TWISState *twis = AT32UC3_TWIS(s->device_states[irq]);

            if(at32uc3_twis_pdca_transfer_complete(twis)) {
                ch->isr |= PDCA_ISR_TRC;
                at32uc3_pdca_update_isr(s, ch);
                break;
            }

            if(!ch->mar_buf) {
                ch->mar_buf = at32uc3_pdca_mar_addr_to_ptr(s, ch);
            }

            uint8_t rhr = at32uc3_twis_pdca_read_rhr(twis);
            *ch->mar_buf = rhr;
            ++ch->mar_buf;

            printf("[pdca_handle_irq] TWIS_RX mar=0x%x, rhr=0x%x\n", ch->mar, rhr);

            pdca_update_tcv(s, ch);
            break;
        }
        default: {
            printf("[pdca_handle_irq] PDCA hanlding for PID=%d is not implemented\n", irq);
            exit(-1);
            break;
        }
    }

}

static void at32uc3_pdca_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3PDCAState *s = AT32UC3_PDCA(dev);

    sysbus_init_irq(sbd, &s->irq);
    qdev_init_gpio_in(dev, pdca_handle_irq, AT32UC_PDCA_PID_COUNT);

    memory_region_init_io(&s->mmio, OBJECT(s), &pdca_ops, s, TYPE_AT32UC3_PDCA, 0x1000);
    sysbus_init_mmio(sbd, &s->mmio);

    for(int i = 0; i < AT32UC_PDCA_PID_COUNT; ++i) {
        s->device_states[i] = NULL;
        s->active_channels[i] = NULL;
    }
}

static void at32uc3_pdca_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_pdca_realize;
    dc->reset = at32uc3_pdca_reset;
}

static const TypeInfo at32uc3_pdca_info = {
        .name           = TYPE_AT32UC3_PDCA,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3PDCAState),
        .class_init     = at32uc3_pdca_class_init,
};

static void at32uc3_pdca_register_types(void)
{
    type_register_static(&at32uc3_pdca_info);
}

type_init(at32uc3_pdca_register_types)