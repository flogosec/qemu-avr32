/*
 * QEMU AVR32 Nanomind A3200
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
#include "qemu/units.h"
#include "qapi/error.h"
#include "at32uc3.h"
#include "boot.h"
#include "qom/object.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/qdev-properties.h"
#include "at32uc3_twim.h"
struct NanomindA3200MachineState {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/
    AT32UC3CSocState soc;

    MemoryRegion ebi_sdram;
    DeviceState* spi_flash[2];
    DeviceState* fram[2];
};
typedef struct NanomindA3200MachineState NanomindA3200MachineState;

struct NanomindA3200MachineClass {
    /*< private >*/
    MachineClass parent_class;
    /*< public >*/

    int sdram_size;
    int spi_flash_size;
    int config_fram_size;

};
typedef struct NanomindA3200MachineClass NanomindA3200MachineClass;

#define TYPE_NANOMIND_A3200_BASE_MACHINE MACHINE_TYPE_NAME("nanomind-a3200-base")

#define TYPE_NANOMIND_A3200_MACHINE MACHINE_TYPE_NAME("nanomind-a3200")
DECLARE_OBJ_CHECKERS(NanomindA3200MachineState, NanomindA3200MachineClass,
        NANOMIND_A3200_MACHINE, TYPE_NANOMIND_A3200_MACHINE)


static void nanomind_3200_init(MachineState *machine)
{
    SSIBus *ssi;

    NanomindA3200MachineClass* nmmc = NANOMIND_A3200_MACHINE_GET_CLASS(machine);
    nmmc->sdram_size = 32 * MiB;
    nmmc->spi_flash_size = 128 * MiB;
    nmmc->config_fram_size = 32 * KiB;

    NanomindA3200MachineState* nmms = NANOMIND_A3200_MACHINE(machine);

    printf("Setting up board...\n");

    object_initialize_child(OBJECT(machine), "soc", &nmms->soc, TYPE_AT32UC3C0512C_SOC);
    sysbus_realize(SYS_BUS_DEVICE(&nmms->soc), &error_abort);

    const int spi_num_buses = 2;
    for(int i = 0; i < spi_num_buses; ++i) {
        qemu_irq cs_line;
        ssi = (SSIBus *) qdev_get_child_bus(DEVICE(&nmms->soc.spi[i]), "spi");

        // Create the SPI-connected flash storage
        nmms->spi_flash[i] = qdev_new("s25fl512s");

        // This sets the actual underlying memory interface for the flash chip
        DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);
        if (dinfo) {
            qdev_prop_set_drive_err(nmms->spi_flash[i], "drive",
                                    blk_by_legacy_dinfo(dinfo),
                                    &error_fatal);
        }
        qdev_realize_and_unref(nmms->spi_flash[i], BUS(ssi), &error_fatal);

        cs_line = qdev_get_gpio_in_named(nmms->spi_flash[i], SSI_GPIO_CS, 0);
        sysbus_connect_irq(SYS_BUS_DEVICE(&nmms->soc.spi[i]), 0 + 1, cs_line);

        // Create the SPI-connected FRAM
        nmms->fram[i] = qdev_new("fm33256b");
        qdev_realize_and_unref(nmms->fram[i], BUS(ssi), &error_fatal);

        cs_line = qdev_get_gpio_in_named(nmms->fram[i], SSI_GPIO_CS, 0);
        // FRAM fm33256b is at irq 3 + 1
        sysbus_connect_irq(SYS_BUS_DEVICE(&nmms->soc.spi[i]), 3 + 1, cs_line);
    }

    DeviceState* dev;

    // NanoMind Internal I2C
    // 3 Axis Gyroscope
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "mpu3300", 0x69));
    (void) dev;
    // 3 Axis Compass
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "hmc5843", 0x1e));
    (void) dev;
    // Magnetic Sensor
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "rm3100", 0x20));
    (void) dev;
    // Backup RM3100
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "rm3100", 0x48));
    (void) dev;

    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "htpa16x4.eeprom", 0x50));
    (void) dev;
    // Sun Sensor?
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "htpa16x4", 0x60));
    (void) dev;

    // There seem to be multiple GSSB istage things
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "gssb.istage", 0x18));
    (void) dev;
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "gssb.istage", 0x1a));
    (void) dev;
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "gssb.istage", 0x1c));
    (void) dev;
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "gssb.istage", 0x1f));
    (void) dev;
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[2].bus, "gssb.istage", 0x22));
    (void) dev;

    //BUS I2C with CSP implementation
    dev = DEVICE(i2c_slave_create_simple(nmms->soc.twim[0].bus, "nanopower.p31u", 0x2));
    (void) dev;

    printf("Board setup complete\n");
    printf("Loading firmware '%s'...\n", machine->firmware);

    if (machine->firmware) {
        if (!avr32_load_firmware(&nmms->soc.cpu.cpu, machine,
                                 &nmms->soc.sdram, machine->firmware)) {
            exit(1);
        }
    }
}

static void nanomind_3200_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "GomSpace NanoMind A3200";
    mc->alias = "nanomind-a3200";
    mc->init = nanomind_3200_init;
    mc->default_cpus = 1;
    mc->min_cpus = mc->default_cpus;
    mc->max_cpus = mc->default_cpus;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
}

static const TypeInfo nanomind_a3200_machine_types[] = {
        {
                .name           = TYPE_NANOMIND_A3200_MACHINE,
                .parent         = TYPE_MACHINE,
                .instance_size  = sizeof(NanomindA3200MachineState),
                .class_size     = sizeof(NanomindA3200MachineClass),
                .class_init     = nanomind_3200_class_init,
        }
};

DEFINE_TYPES(nanomind_a3200_machine_types)
