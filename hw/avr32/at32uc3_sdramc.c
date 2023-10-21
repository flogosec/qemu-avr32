/*
 * QEMU AVR32 SDRAMC
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
#include "hw/avr32/at32uc3_sdramc.h"

#define MR_REG 0x0
#define RTR_REG 0x4
#define CR_REG 0x8

uint32_t mr_reg = 0;
uint32_t rtr_reg = 0;
uint32_t cr_reg = 0x852372C0;

static void at32uc3_sdramc_reset(DeviceState *dev)
{
    mr_reg = 0;
    rtr_reg = 0;
    cr_reg = 0x852372C0;
}

static uint64_t at32uc_sdramc_read(void *opaque, hwaddr offset, unsigned int size)
{
    switch (offset) {
        case MR_REG:
            printf("[SDRAMC] Read MR: 0x%x\n", mr_reg);
            return mr_reg;
        case RTR_REG:
            printf("[SDRAMC] Read RTR: 0x%x\n", rtr_reg);
            return rtr_reg;
        case CR_REG:
            printf("[SDRAMC] Read CR: 0x%x\n", cr_reg);
            return cr_reg;
        default:
            printf("[SDRAMC] Read on 0x%x\n", (uint32_t) offset);
            return 0;
    }
}

static void at32uc_sdramc_write(void *opaque, hwaddr offset, uint64_t val64, unsigned int size)
{
    switch (offset) {
        case MR_REG:
            printf("[SDRAMC] Write MR: 0x%x\n", (uint32_t)val64);
            mr_reg = (uint32_t)val64;
            break;
        case RTR_REG:
            printf("[SDRAMC] Write RTR: 0x%x\n", (uint32_t)val64);
            rtr_reg = (uint32_t)val64;
            break;
        case CR_REG:
            printf("[SDRAMC] Write CR: 0x%x\n", (uint32_t)val64);
            cr_reg = (uint32_t)val64;
            break;
        default:
            printf("[SDRAMC] Write '0x%x' on 0x%x\n", (uint32_t)val64, (uint32_t)offset);
    }

}

static const MemoryRegionOps sdramc_ops = {
        .read = at32uc_sdramc_read,
        .write = at32uc_sdramc_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

//TODO: add irq
static void at32uc3_sdramc_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3SDRAMCState *s = AT32UC3_SDRAMC(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &sdramc_ops, s, TYPE_AT32UC3_SDRAMC, 0x800);
    sysbus_init_mmio(sbd, &s->mmio);
}

static void at32uc3_sdramc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_sdramc_realize;
    dc->reset = at32uc3_sdramc_reset;
}

static const TypeInfo at32uc3_sdramc_info = {
        .name           = TYPE_AT32UC3_SDRAMC,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3SDRAMCState),
        .class_init     = at32uc3_sdramc_class_init,
};

static void at32uc3_sdramc_register_types(void)
{
    type_register_static(&at32uc3_sdramc_info);
}

type_init(at32uc3_sdramc_register_types)