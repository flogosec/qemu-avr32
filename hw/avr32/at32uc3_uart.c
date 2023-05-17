/*
 * QEMU AVR32 UART
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
#include "hw/avr32/at32uc3_uart.h"
#include "migration/vmstate.h"

static int baudrate = 0;
static FILE *fileOut;

static uint64_t at32uc_uart_read(void *opaque, hwaddr addr, unsigned int size)
{
    int offset = (int) addr;
    int returnValue = 0;
    switch (offset) {
        case 0x4:
            returnValue = baudrate;
            break;
        case 0x14:
            returnValue = 0xFF;
            break;
        default:
            break;
    }
    return returnValue;
}

static void at32uc_uart_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    int offset = (int) addr;
    switch (offset) {
        case 0x4:
            baudrate = val64;
            break;
        case 0x1c:
            if(val64 == 0x30){
                return;
            }
            if(val64 > 0 && val64 < 255){
            }
            else{
            }
            fprintf(fileOut, "%c", (char)val64);
            fflush(fileOut);
            break;
        case 0x0:
        case 0x24:
        case 0x28:
        default:
            break;
    }
}

static const MemoryRegionOps uart_ops = {
        .read = at32uc_uart_read,
        .write = at32uc_uart_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_uart_realize(DeviceState *dev, Error **errp)
{
    fileOut = fopen("uart_output.txt", "w");

    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3UARTState *s = AT32UC3_UART(dev);
    int i;

    sysbus_init_irq(sbd, &s->irq);
    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    for (i = 0; i < s->num_cs; ++i) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &uart_ops, s, TYPE_AT32UC3_UART, 0x100); // R_MAX * 4 = size of region
    sysbus_init_mmio(sbd, &s->mmio);

    s->irqline = -1;
}

static void at32uc3_uart_reset(DeviceState *dev)
{

}

static void at32uc3_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_uart_realize;
    dc->reset = at32uc3_uart_reset;
}

static const TypeInfo at32uc3_uart_info = {
        .name           = TYPE_AT32UC3_UART,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3UARTState),
        .class_init     = at32uc3_uart_class_init,
};

static void at32uc3_uart_register_types(void)
{
    type_register_static(&at32uc3_uart_info);
}

type_init(at32uc3_uart_register_types)