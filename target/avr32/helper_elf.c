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

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "elf.h"
#include <string.h>
#include "target/avr32/helper_elf.h"

int avr32_elf_convert_int(int num){
    uint32_t b0,b1,b2,b3;
    b0 = (num & 0x000000ff) << 24u;
    b1 = (num & 0x0000ff00) << 8u;
    b2 = (num & 0x00ff0000) >> 8u;
    b3 = (num & 0xff000000) >> 24u;
    return b0 | b1 | b2 | b3;
}

uint16_t avr32_elf_convert_short(short num){
    uint32_t b0,b1;
    b0 = (num & 0x00ff) << 8u;
    b1 = (num & 0xff00) >> 8u;
    return b0 | b1;
}

void avr32_convert_elf_header(Elf32_Ehdr *header){
    // We only need some headers
    header->e_machine = avr32_elf_convert_short(header->e_machine);
    header->e_shoff = avr32_elf_convert_int(header->e_shoff);
    header->e_shentsize = avr32_elf_convert_short(header->e_shentsize);
    header->e_shnum = avr32_elf_convert_short(header->e_shnum);
    header->e_shstrndx = avr32_elf_convert_short(header->e_shstrndx);
}

void avr32_elf_read_section_headers(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table){
    fseek(file, header->e_shoff, SEEK_SET);
    int res;
    for(int i= 0; i < header->e_shnum; i++){
        sh_table[i] = malloc(sizeof (Elf32_Shdr));
        res = fread(sh_table[i],  header->e_shentsize, 1, file);
        if(res <= 0){
            error_report("[AVR32-BOOT] Cannot read firmware section table\n");
            exit(1);
        }
        sh_table[i]->sh_offset = avr32_elf_convert_int(sh_table[i]->sh_offset);
        sh_table[i]->sh_size = avr32_elf_convert_int(sh_table[i]->sh_size);
        sh_table[i]->sh_name = avr32_elf_convert_int(sh_table[i]->sh_name);
        sh_table[i]->sh_addr = avr32_elf_convert_int(sh_table[i]->sh_addr);
    }
}

void avr32_elf_read_string_table(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table, char *strtable) {
    int offset = sh_table[header->e_shstrndx]->sh_offset;
    int size = sh_table[header->e_shstrndx]->sh_size;

    int res = 0;
    fseek(file, offset, SEEK_SET);
    res = fread(strtable, size, 1, file);

    if(res <= 0){
        printf("Read error!");
        exit(1);
    }
}

bool avr32_is_elf_file(char *filename){
    FILE *firm_file = fopen(filename, "rb");
    char magic[4];
    int res = fread(&magic, 1, 4, firm_file);
    fclose(firm_file);
    if(!res){
        error_report("[AVR32-BOOT] Cannot read firmware image header\n");
        exit(1);
    }

    return (magic[0] == 0x7f && magic[1] == 0x45 && magic[2] == 0x4c && magic[3] == 0x46);

}