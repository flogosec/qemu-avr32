/*
 * QEMU AVR32 GPIOC
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
#include "hw/avr32/at32uc3_gpioc.h"

#define GPIOC_READ_WRITE 0x0
#define GPIOC_SET 0x4
#define GPIOC_CLEAR 0x8
#define GPIOC_TOGGLE 0xC

#define GPIO_ENABLE_REG (0x00 >> 4)
#define P_MUX0_REG (0x10 >> 4)
#define P_MUX1_REG (0x20 >> 4)
#define P_MUX2_REG (0x30 >> 4)
#define OUTPUT_DRIVER_ENABLE_REG 0x40 >> 4
#define OUTPUT_VALUE_REG 0x50 >> 4
#define PIN_VALUE_REG 0x60 >> 4
#define PULL_UP_ENABLE_REG 0x70 >> 4
#define PULL_DOWN_ENABLE_REG 0x80 >> 4
#define INTERRUPT_ENABLE_REG 0x90 >> 4
#define INTERRUPT_MODE0_REG 0xA0 >> 4
#define INTERRUPT_MODE1_REG 0xB0 >> 4
#define GLITCH_FILTER_ENABLE_REG 0xC0 >> 4
#define INTERRUPT_FLAG_REG 0xD0 >> 4
#define OUTPUT_DRV_CAP0_REG 0x100 >> 4
#define OUTPUT_DRV_CAP1_REG 0x110 >> 4
#define LOCK_REG 0x1A0 >> 4
#define UNLOCK_ACCESS_STAT_REG (0x1E0 >> 4)
#define PARAM_VER_REG (0x1F0 >> 4)

static void at32uc3_gpioc_reset(DeviceState *dev)
{
    AT32UC3GPIOCState *s = AT32UC3_GPIOC(dev);
    for(int i = 0; i< 4; i++){
        for(int j = 0; j < 31; j++){
            s->ports[i].registers[j] = 0;
        }
    }

    s->ports[0].registers[GPIO_ENABLE_REG] = 0x3FF9FFFF;
    s->ports[1].registers[GPIO_ENABLE_REG] = 0xFFFFFFFF;
    s->ports[2].registers[GPIO_ENABLE_REG] = 0xFFFFFFFF;
    s->ports[3].registers[GPIO_ENABLE_REG] = 0x7FFFFFFF;

    s->ports[0].registers[P_MUX0_REG] = 0x00000001;
    s->ports[1].registers[P_MUX0_REG] = 0x00000002;

    s->ports[0].registers[PULL_UP_ENABLE_REG] = 0x00000001;

    s->ports[0].registers[GLITCH_FILTER_ENABLE_REG] = 0x3FF9FFFF;
    s->ports[1].registers[GLITCH_FILTER_ENABLE_REG] = 0xFFFFFFFF;
    s->ports[2].registers[GLITCH_FILTER_ENABLE_REG] = 0xFFFFFFFF;
    s->ports[3].registers[GLITCH_FILTER_ENABLE_REG] = 0x7FFFFFFF;

    //Parameter registers
    s->ports[0].registers[PARAM_VER_REG + 8] = 0x3FF9FFFF;
    s->ports[1].registers[PARAM_VER_REG + 8] = 0x3FFFFFFF;
    s->ports[2].registers[PARAM_VER_REG + 8] = 0xFFFFFFFF;
    s->ports[3].registers[PARAM_VER_REG + 8] = 0x7FFFFFFF;

    //Version registers
    s->ports[0].registers[PARAM_VER_REG + 0xC] = 0x00000212;
    s->ports[1].registers[PARAM_VER_REG + 0xC] = 0x00000212;
    s->ports[2].registers[PARAM_VER_REG + 0xC] = 0x00000212;
    s->ports[3].registers[PARAM_VER_REG + 0xC] = 0x00000212;
}

static uint64_t at32uc_gpioc_read(void *opaque, hwaddr offset, unsigned int size)
{
    AT32UC3GPIOCState* s = AT32UC3_GPIOC(opaque);

    uint32_t addr = (uint32_t)offset;
    int port;
    if(addr <0x200){
        port = 0;
    }
    else if(addr < 0x400)  {
        port = 1;
    }
    else if(addr < 0x600)  {
        port = 2;
    }
    else{
        port = 3;
    }

    //TODO: this calculation is not working, as the "reg number" is not aligned. 0x110 is followed by 0x1a0
    //Workaround is using 64 registers instead of 21
    int reg = addr & 0xFF0;
    reg -= port * 0x200;
    reg = reg >> 4;
    int operation = addr & 0x00F;

    if(operation != GPIOC_READ_WRITE){
        printf("[GPIOC_read] Non-read operation on read-only register 0x%x!\n", reg);
        return 0xff;
    }

    //TODO: Read values from future peripheral devices
    return s->ports[port].registers[reg];
}

static void at32uc_gpioc_write(void *opaque, hwaddr offset, uint64_t val64, unsigned int size)
{
    AT32UC3GPIOCState* s = opaque;
    uint32_t addr = (uint32_t)offset;
    int port;
    if(addr <0x200){
        port = 0;
    }
    else if(addr < 0x400)  {
        port = 1;
    }
    else if(addr < 0x600)  {
        port = 2;
    }
    else{
        port = 3;
    }

    //TODO: this calculation is not working, as the "reg number" is not aligned. 0x110 is followed by 0x1a0
    //Workaround is using 64 registers instead of 21
    int reg = addr & 0xFF0;
    reg -= port * 0x200;
    reg = reg >> 4;
    int operation = addr & 0x00F;

    if(reg == PARAM_VER_REG){
        printf("[GPIOC_write] Write to read-only Parameter/Version register!\n");
        return;
    }

    printf("[GPIOC_write] Port: %i, ", port);
    switch(reg) {
        case GPIO_ENABLE_REG: {
            printf("GPIO_ENABLE_REG ");
            break;
        }
        case P_MUX0_REG: {
            printf("P_MUX0_REG ");
            break;
        }
        case P_MUX1_REG: {
            printf("P_MUX1_REG ");
            break;
        }
        case P_MUX2_REG: {
            printf("P_MUX2_REG ");
            break;
        }
        case OUTPUT_DRIVER_ENABLE_REG: {
            printf("OUTPUT_DRIVER_ENABLE_REG ");
            break;
        }
        case OUTPUT_VALUE_REG: {
            printf("OUTPUT_VALUE_REG ");
            break;
        }
        case PULL_UP_ENABLE_REG: {
            printf("PULL_UP_ENABLE_REG ");
            break;
        }
        case PULL_DOWN_ENABLE_REG: {
            printf("PULL_DOWN_ENABLE_REG ");
            break;
        }
        case OUTPUT_DRV_CAP0_REG: {
            printf("OUTPUT_DRV_CAP0_REG ");
            break;
        }
        default: {
            printf("NA (0x%x) op: ", reg);
            break;
        }
    }
    switch(operation) {
        case GPIOC_READ_WRITE: {
            printf("WRITE: 0x%x\n", (uint32_t)val64);
            s->ports[port].registers[reg] = (uint32_t)val64;
            break;
        }
        case GPIOC_SET: {
            printf("SET: 0x%x\n", (uint32_t)val64);
            s->ports[port].registers[reg] |= (uint32_t)val64;
            break;
        }
        case GPIOC_CLEAR: {
            s->ports[port].registers[reg] &= ~(uint32_t)val64;
            printf("CLEAR: 0x%x, new val: 0x%04x\n", (uint32_t)val64, s->ports[port].registers[reg]);
            break;
        }
        case GPIOC_TOGGLE: {
            for(int i = 0; i < 32; i++){
                if(val64 & ((uint32_t)1 << i)){
                    //TODO: add test to testing framework
                    s->ports[port].registers[reg] ^= ((uint32_t)1 << i);
                }
            }
            printf("TOGGLE: 0x%x, new val: 0x04%x\n", (uint32_t)val64, s->ports[port].registers[reg]);
            break;
        }
    }
}

static const MemoryRegionOps gpioc_ops = {
        .read = at32uc_gpioc_read,
        .write = at32uc_gpioc_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

//TODO: add irq
static void at32uc3_gpioc_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3GPIOCState *s = AT32UC3_GPIOC(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &gpioc_ops, s, TYPE_AT32UC3_GPIOC, 0x800);
    sysbus_init_mmio(sbd, &s->mmio);
}

static void at32uc3_gpioc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_gpioc_realize;
    dc->reset = at32uc3_gpioc_reset;
}

static const TypeInfo at32uc3_gpioc_info = {
        .name           = TYPE_AT32UC3_GPIOC,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3GPIOCState),
        .class_init     = at32uc3_gpioc_class_init,
};

static void at32uc3_gpioc_register_types(void)
{
    type_register_static(&at32uc3_gpioc_info);
}

type_init(at32uc3_gpioc_register_types)