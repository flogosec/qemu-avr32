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
#ifndef HW_AVR32_BOOT_H
#define HW_AVR32_BOOT_H

#include "hw/boards.h"
#include "cpu.h"
#include "elf.h"

/**
 * avr32_load_firmware:   load an image into a memory region
 *
 * @cpu:        Handle a AVR CPU object
 * @ms:         A MachineState
 * @mr:         Memory Region to load into
 * @firmware:   Path to the firmware file (raw binary or ELF format)
 *
 * Load a firmware supplied by the machine or by the user  with the
 * '-bios' command line option, and put it in target memory.
 *
 * Returns: true on success, false on error.
 */
bool avr32_load_firmware(AVR32ACPU *cpu, MachineState *ms,
                         MemoryRegion *mr, const char *firmware);
bool avr32_load_elf_file(AVR32ACPU *cpu, const char *filename, MemoryRegion *program_mr);
void avr32_copy_sections(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *sh_strtable, MemoryRegion *program_mr);
void avr32_copy_text_section(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *sh_strtable, FILE* output);
void avr32_copy_data_section(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *sh_strtable, FILE* output);

#endif // HW_AVR32_BOOT_H
