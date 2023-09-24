/*
 * QEMU AVR32 ELF Helper
 *
 * Copyright (c) 2023 Florian Göhler
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
#ifndef QEMU_AVR32_HELPER_ELF_H
#define QEMU_AVR32_HELPER_ELF_H

#include "elf.h"

bool avr32_is_elf_file(const char *filename);
void avr32_convert_elf_header(Elf32_Ehdr *header);
uint32_t avr32_elf_convert_int(uint32_t num);
uint16_t avr32_elf_convert_short(short num);
void avr32_elf_read_section_headers(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table);
void avr32_elf_read_sh_string_table(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table, char *sh_strtable);
void avr32_elf_read_string_table(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table, char *sh_strtable, char *strtable);
void avr32_read_symtab(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table, char *sh_strtable, char *strtab, Elf32_Sym** sym_tab);
extern const char *AVR32_FIRMWARE_FILE;

#endif //QEMU_AVR32_HELPER_ELF_H
