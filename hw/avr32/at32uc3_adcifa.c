/*
 * QEMU AVR32 ADCIFA
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
#include "hw/avr32/at32uc3_adcifa.h"
#include "migration/vmstate.h"

static uint64_t at32uc_adcifa_read(void *opaque, hwaddr addr, unsigned int size)
{

    int offset = (int) addr;
    int returnValue = 0;
    switch (offset) {
        case 0x0008:
            returnValue = 0xFFFF;
            break;
        default:
            break;
    }

    return returnValue;
}

static void at32uc_adcifa_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{

}

static const MemoryRegionOps adcifa_ops = {
        .read = at32uc_adcifa_read,
        .write = at32uc_adcifa_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_adcifa_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3ADCIFAState *s = AT32UC3_ADCIFA(dev);
    int i;

    sysbus_init_irq(sbd, &s->irq);
    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    for (i = 0; i < s->num_cs; ++i) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &adcifa_ops, s, TYPE_AT32UC3_ADCIFA, 0x100); // R_MAX * 4 = size of region
    sysbus_init_mmio(sbd, &s->mmio);

    s->irqline = -1;
}

static void at32uc3_adcifa_reset(DeviceState *dev)
{
}

static void at32uc3_adcifa_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_adcifa_realize;
    dc->reset = at32uc3_adcifa_reset;
}

static const TypeInfo at32uc3_adcifa_info = {
        .name           = TYPE_AT32UC3_ADCIFA,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3ADCIFAState),
        .class_init     = at32uc3_adcifa_class_init,
};

static void at32uc3_adcifa_register_types(void)
{
    type_register_static(&at32uc3_adcifa_info);
}

type_init(at32uc3_adcifa_register_types)