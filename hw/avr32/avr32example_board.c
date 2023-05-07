/*
 * QEMU AVR32 Example board
 *
 * Copyright (c) 2022-2023 Florian Göhler
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
#include "avr32exp.h"
#include "boot.h"
#include "qom/object.h"
#include "hw/boards.h"

struct AVR32ExampleBoardMachineState {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/
    AVR32EXPMcuState mcu;

};
typedef struct AVR32ExampleBoardMachineState AVR32ExampleBoardMachineState;

struct AVR32ExampleBoardMachineClass {
    /*< private >*/
    MachineClass parent_class;
    /*< public >*/
};
typedef struct AVR32ExampleBoardMachineClass AVR32ExampleBoardMachineClass;

#define TYPE_AVR32EXAMPLE_BOARD_BASE_MACHINE MACHINE_TYPE_NAME("avr32example-board-base")

#define TYPE_AVR32EXAMPLE_BOARD_MACHINE MACHINE_TYPE_NAME("avr32example-board")
DECLARE_OBJ_CHECKERS(AVR32ExampleBoardMachineState, AVR32ExampleBoardMachineClass,
        AVR32EXAMPLE_BOARD_MACHINE, TYPE_AVR32EXAMPLE_BOARD_MACHINE)


static void avr32example_board_init(MachineState *machine)
{
    AVR32ExampleBoardMachineState* m_state = AVR32EXAMPLE_BOARD_MACHINE(machine);

    printf("Setting up board...\n");

    object_initialize_child(OBJECT(machine), "mcu", &m_state->mcu, TYPE_AVR32EXPS_MCU);
    sysbus_realize(SYS_BUS_DEVICE(&m_state->mcu), &error_abort);


    printf("Board setup complete\n");
    printf("Loading firmware '%s'...\n", machine->firmware);

    if (machine->firmware) {
        if (!avr32_load_firmware(&m_state->mcu.cpu, machine,
                                 &m_state->mcu.flash, machine->firmware)) {
            exit(1);
        }
    }
}

static void avr32example_board_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "AVR32 Example Board";
    mc->alias = "avr32example-board";
    mc->init = avr32example_board_init;
    mc->default_cpus = 1;
    mc->min_cpus = mc->default_cpus;
    mc->max_cpus = mc->default_cpus;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
}

static const TypeInfo avr32example_board_machine_types[] = {
        {
                .name           = TYPE_AVR32EXAMPLE_BOARD_MACHINE,
                .parent         = TYPE_MACHINE,
                .instance_size  = sizeof(AVR32ExampleBoardMachineState),
                .class_size     = sizeof(AVR32ExampleBoardMachineClass),
                .class_init     = avr32example_board_class_init,
        }
};

DEFINE_TYPES(avr32example_board_machine_types)
