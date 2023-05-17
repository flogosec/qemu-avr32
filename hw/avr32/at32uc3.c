/*
 * QEMU AVR32UC3 MCU
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
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "exec/address-spaces.h"
#include "at32uc3.h"
#include "hw/qdev-properties.h"
#include "sysemu/sysemu.h"
#include "hw/avr32/at32uc3_twim.h"
#include "hw/avr32/at32uc3_pdca.h"
#include "hw/avr32/at32uc3_adcifa.h"
#include "hw/avr32/at32uc3_uart.h"
#include "hw/avr32/at32uc3_can.h"

static const uint32_t spi_addr[AT32UC3C_MAX_SPIS] = { 0xFFFD1800, 0xFFFF3400 };
static const int spi_irq[AT32UC3C_MAX_SPIS] = {3, 28};

static const uint32_t pdca_addr = 0xfffd0000;
static const uint32_t can_addr = 0xfffd1c00;
static const uint32_t tc_addr = 0xfffd2000;
static const uint32_t adcifa_addr = 0xfffd2400;
static const uint32_t uart_addr = 0xfffd2800;
static const uint32_t twim_addr[AT32UC3C_MAX_TWI] = { 0xFFFF3800, 0xFFFF3c00, 0xFFFD2c00};
static const uint32_t twis_addr[AT32UC3C_MAX_TWI] = { 0xFFFF4000, 0xFFFF4400, 0xFFFD3000};
static const uint32_t intc_addr = 0xffff0000;
static const uint32_t scif_addr = 0xffff0800;

// TODO The IRQ numbers are just arbitrary
static const int timer_irq = AT32UC3C_IRQ_TC02;
static const int twim_irq[AT32UC3C_MAX_TWI] = {AT32UC3C_IRQ_TWIM0, AT32UC3C_IRQ_TWIM1, AT32UC3C_IRQ_TWIM2};
static const int twis_irq[AT32UC3C_MAX_TWI] = {AT32UC3C_IRQ_TWIS0, AT32UC3C_IRQ_TWIS1, AT32UC3C_IRQ_TWIS2};
static const int pdca_irq = 16;
static const int adcifa_irq = 17;
static const int uart_irq = 18;
static const int can_irq = 19;
static const int scif_irq = 20;

const int PDCA_TWIS_RX_PIDS[] = {AT32UC_PDCA_PID_TWIS0_RX, AT32UC_PDCA_PID_TWIS1_RX, AT32UC_PDCA_PID_TWIS2_RX};
const int PDCA_TWIM_RX_PIDS[] = {AT32UC_PDCA_PID_TWIM0_RX, AT32UC_PDCA_PID_TWIM1_RX, AT32UC_PDCA_PID_TWIM2_RX};
const int PDCA_TWIM_TX_PIDS[] = {AT32UC_PDCA_PID_TWIM0_TX, AT32UC_PDCA_PID_TWIM1_TX, AT32UC_PDCA_PID_TWIM2_TX};


struct AT32UC3CSocClass {
    /*< private >*/
    SysBusDeviceClass parent_class;
//    DeviceClass parent_obj;

    /*< public >*/
    const char *cpu_type;

    size_t flash_size;
    size_t sram_size;

    size_t max_sdram_size;
    size_t can_count;
    size_t usart_count;
    size_t spi_count;

    size_t timer_count;
    size_t twim_count;
    size_t twis_count;
    size_t adcifa_count;
    size_t uart_count;
    size_t scif_count;
    size_t intc_count;
};

typedef struct AT32UC3CSocClass AT32UC3CSocClass;

DECLARE_CLASS_CHECKERS(AT32UC3CSocClass, AT32UC3C_SOC, TYPE_AT32UC3C_SOC)


// This functions sets up the device
static void at32uc3_realize(DeviceState *dev_soc, Error **errp)
{
    int i;
    DeviceState *dev, *intc_dev, *pdca_dev;
    SysBusDevice *busdev;

    AT32UC3CSocState *s = AT32UC3C_SOC(dev_soc);

    const AT32UC3CSocClass *mc = AT32UC3C_SOC_GET_CLASS(dev_soc);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->cpu), errp)) {
        return;
    }

    s->cpu.cpu.env.intc = &s->intc;

    /* SRAM */
    memory_region_init_ram(&s->sram, OBJECT(dev_soc), "sram", mc->sram_size, &error_abort);
    memory_region_add_subregion(get_system_memory(),  0x00000000, &s->sram);

    /* Flash */
    memory_region_init_rom(&s->onChipFlash, OBJECT(dev_soc), "flash", mc->flash_size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0x80000000, &s->onChipFlash);

    /* RAM-Image */
    // TODO: Workaround to load firmware
    memory_region_init_ram(&s->sdram, OBJECT(dev_soc), "sdram", 32*MiB, &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0xd0000000, &s->sdram);

    /* SYS Stack */
    memory_region_init_ram(&s->sysstack, OBJECT(dev_soc), "sysstack", 0x40000, &error_abort);
    memory_region_add_subregion(get_system_memory(),  0x4FFC0000, &s->sysstack);

    /* INTC (Interrupt Controller) */
    intc_dev = DEVICE(&(s->intc));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->intc), errp)) {
        return;
    }
    s->intc.cpu = &s->cpu.cpu;
    busdev = SYS_BUS_DEVICE(intc_dev);
    sysbus_mmio_map(busdev, 0, intc_addr);

    /* SPI */
    for (i = 0; i < mc->spi_count; ++i) {
        dev = DEVICE(&(s->spi[i]));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, spi_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev, spi_irq[i]));
    }

    /* timer */
    dev = DEVICE(&(s->timer));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->timer), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, tc_addr);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev, timer_irq));

    /* pdca */
    pdca_dev = DEVICE(&(s->pdca));
    s->pdca.ram = &s->sdram;

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->pdca), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(pdca_dev);
    sysbus_mmio_map(busdev, 0, pdca_addr);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev, pdca_irq));

    // TWIM
    for(i = 0; i < mc->twim_count; ++i) {
        dev = DEVICE(&(s->twim[i]));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->twim[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, twim_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev, twim_irq[i]));

        s->twim[i].pdca = &s->pdca;
        s->twim[i].pdca_recv_pid = PDCA_TWIM_RX_PIDS[i];
        s->twim[i].pdca_send_pid = PDCA_TWIM_TX_PIDS[i];
    }

    // TWIS
    for(i = 0; i < mc->twis_count; ++i) {
        dev = DEVICE(&(s->twis[i]));

        s->twis[i].bus = s->twim[i].bus;
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->twis[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, twis_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev, twis_irq[i]));

        sysbus_connect_irq(busdev, 1, qdev_get_gpio_in(pdca_dev, PDCA_TWIS_RX_PIDS[i]));
        s->pdca.device_states[PDCA_TWIS_RX_PIDS[i]] = dev;
    }

    /* adcifa */
    dev = DEVICE(&(s->adcifa));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->adcifa), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, adcifa_addr);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev, adcifa_irq));

    /* uart */
    dev = DEVICE(&(s->uart));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, uart_addr);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev,uart_irq));

    /* can */
    dev = DEVICE(&(s->can));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->can), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, can_addr);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev,can_irq));

    /* scif */
    dev = DEVICE(&(s->scif));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->scif), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, scif_addr);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(intc_dev,scif_irq));
}
static void at32uc3c_inst_init(Object *obj)
{
    int i;
    AT32UC3CSocState *s = AT32UC3C_SOC(obj);
    const AT32UC3CSocClass *mc = AT32UC3C_SOC_GET_CLASS(s);

    object_initialize_child(obj, "avr32uc", &s->cpu, TYPE_AVR32UC);

    for (i = 0; i < mc->spi_count; i++) {
        object_initialize_child(obj, "spi[*]", &s->spi[i], TYPE_AT32UC3_SPI);
    }

    for (i = 0; i < mc->twim_count; i++) {
        object_initialize_child(obj, "twim[*]", &s->twim[i], TYPE_AT32UC3_TWIM);
    }

    for (i = 0; i < mc->twis_count; i++) {
        object_initialize_child(obj, "twis[*]", &s->twis[i], TYPE_AT32UC3_TWIS);
    }

    object_initialize_child(obj, "timer", &s->timer, TYPE_AT32UC3_TIMER);
    object_initialize_child(obj, "pdca", &s->pdca, TYPE_AT32UC3_PDCA);
    object_initialize_child(obj, "adcifa", &s->adcifa, TYPE_AT32UC3_ADCIFA);
    object_initialize_child(obj, "uart", &s->uart, TYPE_AT32UC3_UART);
    object_initialize_child(obj, "can", &s->can, TYPE_AT32UC3_CAN);
    object_initialize_child(obj, "scif", &s->scif, TYPE_AT32UC3_SCIF);
    object_initialize_child(obj, "intc", &s->intc, TYPE_AT32UC3_INTC);
}

static void at32uc3c_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = at32uc3_realize;
    dc->user_creatable = false;
}

static void at32uc3c0512c_class_init(ObjectClass *oc, void *data)
{
    AT32UC3CSocClass* at32uc3 = AT32UC3C_SOC_CLASS(oc);

    at32uc3->cpu_type = AVR32A_CPU_TYPE_NAME("AT32UC3C");
    at32uc3->flash_size = 512 * KiB;
    at32uc3->sram_size = 68 * KiB;
    at32uc3->max_sdram_size = 4096 * MiB;
    at32uc3->can_count = 2;
    at32uc3->usart_count = 4;
    at32uc3->spi_count = 2;
    at32uc3->timer_count = 1;
    at32uc3->twim_count = 3;
    at32uc3->twis_count = 3;
    at32uc3->adcifa_count = 1;
    at32uc3->uart_count = 1;
    at32uc3->scif_count = 1;
    at32uc3->intc_count = 1;
}

static const TypeInfo at32uc3c_soc_types[] = {
        {
                .name           = TYPE_AT32UC3C0512C_SOC,
                .parent         = TYPE_AT32UC3C_SOC,
                .class_init     = at32uc3c0512c_class_init,
        }, {
                .name           = TYPE_AT32UC3C_SOC,
                .parent         = TYPE_SYS_BUS_DEVICE,
                .instance_size  = sizeof(AT32UC3CSocState),
                .instance_init  = at32uc3c_inst_init,
                .class_size     = sizeof(AT32UC3CSocClass),
                .class_init     = at32uc3c_class_init,
                .abstract       = true,
        }
};

DEFINE_TYPES(at32uc3c_soc_types)