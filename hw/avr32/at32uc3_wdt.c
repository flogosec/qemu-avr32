/*
 * QEMU AVR32 WDT
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
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/avr32/at32uc3.h"
#include "migration/vmstate.h"
#include "qemu/timer.h"
#include "sys/time.h"
#include "qom/object.h"
#include "qemu/main-loop.h"
#include "threads.h"
#include "sysemu/watchdog.h"

#define WDT_CTRL (0x000)
#define WDT_CLR (0x004)
#define WDT_SR (0x008)
#define WDT_VERSION (0x3FC)

#define WDT_CTRL_KEY_MASK (0xFF << 24)
#define WDT_CTRL_CONTENT (0x00FFFFFF)
#define WDT_CTRL_TBAN_MASK (0x1F << 18)
#define WDT_CTRL_CSSEL BIT(17)
#define WDT_CTRL_CEN BIT(16)
#define WDT_CTRL_PSEL_MASK (0x1F << 8)
#define WDT_CTRL_FCD BIT(7)
#define WDT_CTRL_SFV BIT(3)
#define WDT_CTRL_MODE BIT(2)
#define WDT_CTRL_DAR BIT(1)
#define WDT_CTRL_EN BIT(0)
#define WDT_CLR_KEY_MASK (0xFF << 24)
#define WDT_CLR_WDTCLR BIT(0)
#define WDT_SR_WINDOW BIT(0)
#define WDT_SR_CLEARED BIT(1)

static void at32uc3_wdt_reset(DeviceState *dev)
{
    AT32UC3WDTState *s = AT32UC3_WDT(dev);
    s->ctrl = 0x00010080;
    s->ctrl_last = 0;
    s->clr = 0x0;
    s->sr = 0x3;
}

static uint64_t at32uc_wdt_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3WDTState* s = AT32UC3_WDT(opaque);
    switch(addr) {
        case WDT_CTRL: {
            printf("[at32uc_wdt_read] CTRL: 0x%04x\n", s->ctrl);
            return s->ctrl;
        }
        case WDT_CLR: {
            printf("[at32uc_wdt_read] CLR is write-only\n");
            return 0xdeadbeef;
        }
        case WDT_SR: {
//            printf("[at32uc_wdt_read] SR: 0x%04x\n", s->sr);
            return s->sr;
        }
        case WDT_VERSION: {
            return 0x00000410;
        }
        default: {
            printf("[at32uc_wdt_read] addr=0x%lx is not implemented!\n", addr);
            exit(-1);
            return 0xdeadbeef;
        }
    }
}

static void at32uc_wdt_write(void *opaque, hwaddr offset, uint64_t val64, unsigned int size)
{
    AT32UC3WDTState* s = opaque;
    switch(offset) {
        case WDT_CTRL: {
            if ((val64 & WDT_CTRL_KEY_MASK) == 0x55000000) {
                s->ctrl_last = val64;
                return;
            }
            if ((val64 & WDT_CTRL_KEY_MASK) == 0xAA000000 && (s->ctrl_last & WDT_CTRL_KEY_MASK) == 0x55000000){
                if ((val64 & WDT_CTRL_CONTENT) == (s->ctrl_last & WDT_CTRL_CONTENT)) {
                    uint32_t new_ctrl = val64 & WDT_CTRL_CONTENT;
                    s->ctrl = new_ctrl;
                    printf("[at32uc_wdt_write] CTRL: 0x%08x\n", s->ctrl);
                } else {
                    printf("[at32uc_wdt_write] CTRL invalid!\n");
                }
                s->ctrl_last = 0;
                return;
            }
            break;
        }
        case WDT_CLR: {
            if (s->clr == 0x55000001 && val64  == 0xAA000001)
            {
                uint32_t timeout = 1 << (((s->ctrl & WDT_CTRL_PSEL_MASK) >> 8) + 1);
                s->clr = 0;
                s->sr = 0;
                ptimer_transaction_begin(s->timer);
                ptimer_stop(s->timer);
                ptimer_set_freq(s->timer, 32000);
                ptimer_set_limit(s->timer, timeout, 1);
                if(!(s->ctrl & WDT_CTRL_DAR))
                    ptimer_run(s->timer, 1);
                ptimer_transaction_commit(s->timer);
                s->sr = WDT_SR_CLEARED | WDT_SR_WINDOW;
            }
            else
            {
                s->clr = val64;
            }
            break;
        }
        case WDT_SR:
        {
            printf("[at32uc_wdt_read] SR is read-only!\n");
            return;
        }
        case WDT_VERSION: {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: write to read-only reg at offset 0x%"
            HWADDR_PRIx "\n", __func__, offset);
            break;
        }
        default: {
            qemu_log_mask(LOG_UNIMP,
                          "%s: uninmplemented write at offset 0x%" HWADDR_PRIx "\n",
                    __func__, offset);
            break;
        }
    }
}

static void at32uc3_wdt_expired(void *opaque)
{
    AT32UC3WDTState *s = AT32UC3_WDT(opaque);

    bool enabled = s->ctrl & WDT_CTRL_EN;

    /* Perform watchdog action if watchdog is enabled and can trigger reset */
    if (enabled) {
        printf("[at32uc_wdt] EXPIRED\n");
        watchdog_perform_action();
    }
}

static const MemoryRegionOps wdt_ops = {
        .read = at32uc_wdt_read,
        .write = at32uc_wdt_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_wdt_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3WDTState *s = AT32UC3_WDT(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &wdt_ops, s, TYPE_AT32UC3_WDT, 0x400);
    sysbus_init_mmio(sbd, &s->mmio);

    s->timer = ptimer_init(at32uc3_wdt_expired, s,
                           PTIMER_POLICY_NO_IMMEDIATE_TRIGGER |
                           PTIMER_POLICY_NO_IMMEDIATE_RELOAD |
                           PTIMER_POLICY_NO_COUNTER_ROUND_DOWN);

    ptimer_transaction_begin(s->timer);
    ptimer_set_freq(s->timer, 2);
    ptimer_set_limit(s->timer, 0xff, 1);
    ptimer_transaction_commit(s->timer);
}

static void at32uc3_wdt_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_wdt_realize;
    dc->reset = at32uc3_wdt_reset;
}

static const TypeInfo at32uc3_wdt_info = {
        .name           = TYPE_AT32UC3_WDT,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3WDTState),
        .class_init     = at32uc3_wdt_class_init,
};

static void at32uc3_wdt_register_types(void)
{
    type_register_static(&at32uc3_wdt_info);
}

type_init(at32uc3_wdt_register_types)