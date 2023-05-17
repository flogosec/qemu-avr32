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

#ifndef HW_AVR32_AT32UC3C_H
#define HW_AVR32_AT32UC3C_H

#include "hw/sysbus.h"

#include "hw/avr32/avr32uc.h"
#include "hw/ssi/at32uc3_spi.h"
#include "hw/timer/at32uc3_timer.h"
#include "hw/avr32/at32uc3_twim.h"
#include "hw/avr32/at32uc3_twis.h"
#include "hw/avr32/at32uc3_pdca.h"
#include "hw/avr32/at32uc3_adcifa.h"
#include "hw/avr32/at32uc3_uart.h"
#include "hw/avr32/at32uc3_can.h"
#include "hw/avr32/at32uc3_scif.h"
#include "hw/avr32/at32uc3_intc.h"

#define AT32UC3C_MAX_SPIS 2
#define AT32UC3C_MAX_TWI 3

#define AT32UC3C_IRQ_TC02 10

#define AT32UC3C_IRQ_TWIM0 15
#define AT32UC3C_IRQ_TWIM1 25
#define AT32UC3C_IRQ_TWIM2 26

#define AT32UC3C_IRQ_TWIS0 22
#define AT32UC3C_IRQ_TWIS1 23
#define AT32UC3C_IRQ_TWIS2 24

#define TYPE_AT32UC3C_SOC "AT32UC3C"
#define TYPE_AT32UC3C0512C_SOC "AT32UC3C0512C"

typedef struct AT32UC3CSocState AT32UC3CSocState;
DECLARE_INSTANCE_CHECKER(AT32UC3CSocState, AT32UC3C_SOC, TYPE_AT32UC3C_SOC)

struct AT32UC3CSocState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    AVR32UCState cpu;

    MemoryRegion onChipFlash;
    MemoryRegion sdram;
    MemoryRegion sram;
    MemoryRegion sysstack;

    qemu_irq irq;

    AT32UC3SPIState spi[AT32UC3C_MAX_SPIS];
    AT32UC3TIMERState timer;
    AT32UC3TWIMState twim[AT32UC3C_MAX_TWI];
    AT32UC3TWISState twis[AT32UC3C_MAX_TWI];
    AT32UC3PDCAState pdca;
    AT32UC3ADCIFAState adcifa;
    AT32UC3UARTState uart;
    AT32UC3UARTState uart1;
    AT32UC3CANState can;
    AT32UC3SCIFState scif;
    AT32UC3INTCState intc;
};


#endif // HW_AVR32_AT32UC3C_H