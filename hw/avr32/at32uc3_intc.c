/*
 * QEMU AVR32 INTC
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
#include "hw/avr32/at32uc3_intc.h"
#include "migration/vmstate.h"
#include "avr32uc.h"
#include "hw/sysbus.h"
#include "at32uc3.h"

static const uint8_t IRQ_GRP_LINE[][2] = {
        [0] = {0xff, 0xff},
        [1] = {0xff, 0xff},
        [2] = {0xff, 0xff},
        [3] = {0xff, 0xff},
        [4] = {0xff, 0xff},
        [5] = {0xff, 0xff},
        [6] = {0xff, 0xff},
        [7] = {0xff, 0xff},
        [8] = {0xff, 0xff},
        [9] = {0xff, 0xff},
        [AT32UC3C_IRQ_TC02] = {33, 2},
        [11] = {0xff, 0xff},
        [12] = {0xff, 0xff},
        [13] = {0xff, 0xff},
        [14] = {0xff, 0xff},
        [AT32UC3C_IRQ_TWIM0] = {25, 0},
        [16] = {0xff, 0xff},
        [17] = {0xff, 0xff},
        [18] = {0xff, 0xff},
        [19] = {0xff, 0xff},
        [20] = {0xff, 0xff},
        [21] = {0xff, 0xff},
        [AT32UC3C_IRQ_TWIS0] = {27, 0},
        [AT32UC3C_IRQ_TWIS1] = {28, 0},
        [AT32UC3C_IRQ_TWIS2] = {46, 0},
        [AT32UC3C_IRQ_TWIM1] = {26, 0},
        [AT32UC3C_IRQ_TWIM2] = {45, 0},
        [27] = {0xff, 0xff},
        [28] = {0xff, 0xff},
        [29] = {0xff, 0xff},
        [30] = {0xff, 0xff},
        [31] = {0xff, 0xff},
};


static void at32uc3_intc_reset(DeviceState *dev)
{
    AT32UC3INTCState* s = AT32UC3_INTC(dev);
    for(int i = 0; i < 64; ++i) {
        s->priority_regs[i] = 0;
    }

    s->grp_req_lines = 0;
    s->val_req_lines = 0;
}

static uint64_t at32uc_intc_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3INTCState* s = AT32UC3_INTC(opaque);

    if(addr < 0x100) {
        return s->priority_regs[addr >> 2];
    } else if (addr < 0x200) {
        return s->request_regs[(addr - 0x100) >> 2];
    } else if (addr <= 0x20c) {
        int cause_reg_idx = 3 - ((addr - 0x200) >> 2);
        return s->cause[cause_reg_idx];
    } else {
        printf("[at32uc_intc_read] Unknown reading unknown addr=0x%lx\n", addr);
        return 0xdeadbeef;
    }
}

static void at32uc_intc_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size) {
    AT32UC3INTCState* s = AT32UC3_INTC(opaque);

    if(addr >= 0x100) {
        printf("[at32uc_intc_write] interrupt request and cause register are read-only\n");
    } else {
        s->priority_regs[addr >> 2] = val64;
    }
}

#define PENDING_INTO 0
#define PENDING_INT1 1
#define PENDING_INT2 2
#define PENDING_INT3 3
#define IRQ_EXEC_MASKED_GM 0xf1
#define IRQ_EXEC_MASKED_IM 0xff

static int perform_intr_priorization(AT32UC3INTCState* s)
{
    uint8_t intlevel = 0;
    AVR32ACPU *cpu = s->cpu;

    if(cpu->env.sflags[16]) {
        // All interrupts are masked with the Global Interrupt Mask right no
        return IRQ_EXEC_MASKED_GM;
    }

    // Request Masking
    s->val_req_lines = 0;
    for(int i = 0; i < 64; ++i) {
        if(s->grp_req_lines & (1l << i)) {
            intlevel = (s->priority_regs[i] & AT32UC3_INTC_IPR_INTLEVEL) >> 30;

            if(!cpu->env.sflags[17 + intlevel]) {
                s->val_req_lines |= (1l << i);
            }
        }
    }

    // Prioritization
    // Check which grp with an active ValReqLine has the highest INTLEVEL

    uint8_t intlevel_seen = 0;
    int highest_intlevel = -1;

    for(int i = 0; i < 64; ++i) {
        if(s->val_req_lines & (1l << i)) {
            intlevel = (s->priority_regs[i] & AT32UC3_INTC_IPR_INTLEVEL) >> 30;

            if((intlevel_seen & (1 << intlevel)) == 0) {
                s->cause[intlevel] = i;
                intlevel_seen |= 1 << intlevel;
            }

            if(intlevel > highest_intlevel) {
                highest_intlevel = intlevel;
            }
        }
    }

    if(highest_intlevel == -1) {
        highest_intlevel = IRQ_EXEC_MASKED_IM;
    }

    return highest_intlevel;
}

static void avr32_set_irq(void *opaque, int irq, int level) {
    AT32UC3INTCState* s = AT32UC3_INTC(opaque);
    AVR32ACPU *cpu = s->cpu;
    CPUState *cs = CPU(cpu);

   const uint8_t* grp_line = IRQ_GRP_LINE[irq];
    if(grp_line[0] == 0xff) {
        printf("[avr32_set_irq] ~~~~~~~~~~ Unsupported IRQ=%d ~~~~~~~~~~~~~~~\n", irq);
        exit(-1);
        return;
    }

    if(level > 0) {
        s->request_regs[grp_line[0]] |= 1 << grp_line[1];
        s->grp_req_lines |= 1l << grp_line[0];
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        s->request_regs[grp_line[0]] &= ~(1 << grp_line[1]);

        // Reevaluate the lines
        s->grp_req_lines &= ((uint64_t) !!s->request_regs[grp_line[0]]) << grp_line[0];

        if(!s->grp_req_lines) {
            // No pending interrupt
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
            return;
        }
    }
}

/* Returns the priority reg that refers to the highest intlevel */
uint32_t avr32_intc_get_pending_intr(AT32UC3INTCState* intc)
{
    uint8_t highest_intlevel = perform_intr_priorization(intc);
    if(highest_intlevel <= PENDING_INT3) {
        return intc->priority_regs[intc->cause[highest_intlevel]];
    } else {
        return 0xffffffff;
    }
}

static const MemoryRegionOps intc_ops = {
        .read = at32uc_intc_read,
        .write = at32uc_intc_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_intc_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3INTCState *s = AT32UC3_INTC(dev);
    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->mmio, OBJECT(s), &intc_ops, s, TYPE_AT32UC3_INTC, 0x400);
    sysbus_init_mmio(sbd, &s->mmio);

    qdev_init_gpio_in(dev, avr32_set_irq, 32); // TODO: 32 is just arbitrary
}

static void at32uc3_intc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_intc_realize;
    dc->reset = at32uc3_intc_reset;
}

static const TypeInfo at32uc3_intc_info = {
        .name           = TYPE_AT32UC3_INTC,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3INTCState),
        .class_init     = at32uc3_intc_class_init,
};

static void at32uc3_intc_register_types(void)
{
    type_register_static(&at32uc3_intc_info);
}

type_init(at32uc3_intc_register_types)