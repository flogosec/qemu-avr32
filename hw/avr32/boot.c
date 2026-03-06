/*
 * QEMU AVR32 Boot
 *
 * Copyright (c) 2022-2023 Florian Göhler, Johannes Willbold
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
#include "qemu/datadir.h"
#include "hw/loader.h"
#include "boot.h"
#include "qemu/error-report.h"
#include "elf.h"
#include <string.h>
#include "hw/core/tcg-cpu-ops.h"
#include "exec/exec-all.h"
#include "target/avr32/helper_elf.h"

bool avr32_load_firmware(AVR32ACPU *cpu, MachineState *ms,
                         MemoryRegion *program_mr, const char *firmware)
{
    AVR32_FIRMWARE_FILE = qemu_find_file(QEMU_FILE_TYPE_BIOS, firmware);
    if (AVR32_FIRMWARE_FILE == NULL) {
        error_report("[AVR32-BOOT] Cannot find firmware image '%s'", firmware);
        return false;
    }

    int bytes_loaded;
    uint64_t entry;
    uint32_t e_flags;
    bytes_loaded = load_elf_ram_sym(AVR32_FIRMWARE_FILE,
                                    NULL, NULL, NULL,
                                    &entry, NULL, NULL,
                                    &e_flags, true, EM_AVR32, 0, 0, // EM_AVR32: 185
                                    NULL, true, NULL);
    if (bytes_loaded >= 0) {

    } else {
        printf("[AVR32-BOOT]: Loading firmware images as raw binary\n");
        bytes_loaded = load_image_mr(AVR32_FIRMWARE_FILE, program_mr);
        if (bytes_loaded < 0) {
            error_report("[AVR32-BOOT] Unable to load firmware image %s as raw binary",
                         firmware);
            return false;
        }
    }
    printf("[AVR32-BOOT] Loaded boot image successfully\n");

    return true;
}
