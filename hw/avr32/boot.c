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

static bool avr32_load_elf_file(AVR32ACPU *cpu, char *filename, MemoryRegion *program_mr);
static bool avr32_is_elf_file(char *filename);
static void avr32_convert_elf_header(Elf32_Ehdr *header);
static uint16_t avr32_elf_convert_short(short num);
static void avr32_elf_read_section_headers(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table);
void avr32_elf_read_string_table(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table, char *strtable);
void avr32_copy_sections(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *strtable, MemoryRegion *program_mr);
void avr32_copy_text_section(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *strtable, FILE* output);
void avr32_copy_data_section(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *strtable, FILE* output);



static int avr32_elf_convert_int(int num){
    uint32_t b0,b1,b2,b3;
    b0 = (num & 0x000000ff) << 24u;
    b1 = (num & 0x0000ff00) << 8u;
    b2 = (num & 0x00ff0000) >> 8u;
    b3 = (num & 0xff000000) >> 24u;
    return b0 | b1 | b2 | b3;
}

static uint16_t avr32_elf_convert_short(short num){
    uint32_t b0,b1;
    b0 = (num & 0x00ff) << 8u;
    b1 = (num & 0xff00) >> 8u;
    return b0 | b1;
}

static void avr32_convert_elf_header(Elf32_Ehdr *header){
    // We only need some headers
    header->e_machine = avr32_elf_convert_short(header->e_machine);
    header->e_shoff = avr32_elf_convert_int(header->e_shoff);
    header->e_shentsize = avr32_elf_convert_short(header->e_shentsize);
    header->e_shnum = avr32_elf_convert_short(header->e_shnum);
    header->e_shstrndx = avr32_elf_convert_short(header->e_shstrndx);
}

static void avr32_elf_read_section_headers(Elf32_Ehdr *header, FILE* file, Elf32_Shdr** sh_table){
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

void avr32_copy_text_section(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *strtable, FILE* output){
    int text_section_idx = -1;
    for(int i= 0; i< e_shnum; i++){
        if(strcmp(&strtable[sh_table[i]->sh_name], ".text") == 0){
            text_section_idx = i;
            break;
        }
    }
    if(text_section_idx >= 0){
        printf("[AVR32-BOOT] Text section is at index %i\n", text_section_idx);
    }
    else{
        error_report("[AVR32-BOOT] Unable to find .text section!\n");
        exit(1);
    }

    char *buffer = malloc(sh_table[text_section_idx]->sh_size);
    fseek(file, sh_table[text_section_idx]->sh_offset, SEEK_SET);
    int res = fread(buffer, 1, sh_table[text_section_idx]->sh_size, file);
    printf("[AVR32-BOOT] Loaded 0x%x bytes from .text section\n", res);
    fwrite(buffer, 1, res, output);
    free(buffer);
}

void avr32_copy_data_section(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *strtable, FILE* output){
    int data_section_idx = -1;
    for(int i= 0; i< e_shnum; i++){
        if(strcmp(&strtable[sh_table[i]->sh_name], ".data") == 0){
            data_section_idx = i;
            break;
        }
    }
    if(data_section_idx >= 0){
        printf("[AVR32-BOOT] Data section is at index %i\n", data_section_idx);
    }
    else{
        error_report("[AVR32-BOOT] Unable to find .data section.\n");
        return;
    }
    if(sh_table[data_section_idx]->sh_size <= 0){
        printf("[AVR32-BOOT] Data section has size 0, skipping.\n");
        return;
    }
    int padding_size = sh_table[data_section_idx]->sh_offset - (sh_table[data_section_idx - 1]->sh_offset + sh_table[data_section_idx - 1]->sh_size) - sh_table[data_section_idx]->sh_addr;
    printf("[AVR32-BOOT] Data section padding size: 0x%x\n", padding_size);
    char *buffer = malloc(padding_size);
    memset(buffer, 0, padding_size);
    fwrite(buffer, 1, padding_size, output);
    free(buffer);

    buffer = malloc(sh_table[data_section_idx]->sh_size);
    fseek(file, sh_table[data_section_idx]->sh_offset, SEEK_SET);
    int res = fread(buffer, 1, sh_table[data_section_idx]->sh_size, file);
    printf("[AVR32-BOOT] Loaded 0x%x bytes from .data section\n", res);
    fwrite(buffer, 1, res, output);
    free(buffer);
}

void avr32_copy_sections(int e_shnum, FILE* file, Elf32_Shdr** sh_table, char *strtable, MemoryRegion *program_mr){
    FILE *output = fopen("/tmp/qemu_avr32_tmp_text_sec", "wb");
    avr32_copy_text_section(e_shnum, file, sh_table, strtable, output);
    avr32_copy_data_section(e_shnum, file, sh_table, strtable, output);
    fclose(output);


    int bytes_loaded = load_image_mr("/tmp/qemu_avr32_tmp_text_sec", program_mr);
    if (bytes_loaded < 0) {
        error_report("[AVR32-BOOT] Unable to load firmware image %s as raw binary",
                     "/tmp/qemu_avr32_tmp_text_sec");
        exit(1);
    }
    printf("[AVR32-BOOT] Binary data successfully loaded\n");
    remove("/tmp/qemu_avr32_tmp_text_sec");
    printf("[AVR32-BOOT] Removed temp firmware file\n");
}

bool avr32_load_elf_file(AVR32ACPU *cpu, char *filename, MemoryRegion *program_mr){
    printf("[AVR32-BOOT] Loading firmware images as ELF file\n");

    Elf32_Ehdr header;
    FILE* file = fopen(filename, "rb");
    char *strtable = 0;
    Elf32_Shdr** sh_table = 0;
    if(file) {
        // read the header
        int res = fread(&header, 1, sizeof(header), file);

        if(res <= 0){
            error_report("[AVR32-BOOT] Cannot read firmware image header\n");
            exit(1);
        }
        // check if it's really an elf file
        if (memcmp(header.e_ident, ELFMAG, SELFMAG) == 0) {
            avr32_convert_elf_header(&header);
            if(header.e_machine == EM_AVR32){
                sh_table = malloc(header.e_shnum * sizeof (Elf32_Shdr*));

                avr32_elf_read_section_headers(&header, file, sh_table);
                strtable = (char *)malloc(sh_table[header.e_shstrndx]->sh_size * sizeof(char));

                avr32_elf_read_string_table(&header, file, sh_table, strtable);
                /*for(int i= 0; i< header.e_shnum; i++){
                    printf("Section[%i] size: 0x%x, name: %s\n", i, sh_table[i]->sh_size, &strtable[sh_table[i]->sh_name]);
                }
                */
                avr32_copy_sections(header.e_shnum, file, sh_table, strtable, program_mr);

            }
            else{
                error_report("[AVR32-BOOT] Firmware file is not an AVR32 file!\n");
            }
        }
        else{
            error_report("[AVR32-BOOT] ELF file is not valid!\n");
            exit(1);
        }
    }

    fclose(file);
    free(strtable);
    for(int i= 0; i< header.e_shnum; i++){
        free(sh_table[i]);
    }
    free(sh_table);
    return true;
}

static bool avr32_is_elf_file(char *filename){
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

bool avr32_load_firmware(AVR32ACPU *cpu, MachineState *ms,
                         MemoryRegion *program_mr, const char *firmware)
{
    g_autofree char *filename = NULL;
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, firmware);
    if (filename == NULL) {
        error_report("[AVR32-BOOT] Cannot find firmware image '%s'", firmware);
        return false;
    }

    if(avr32_is_elf_file(filename)){
        //TODO: For some reason QEMU internal ELF-loaders fail to load an AVR32 Elf file. For now we use a custom elf-loader to extract the .text section from an elf-file.
        avr32_load_elf_file(cpu, filename, program_mr);
    }
    else{
        printf("[AVR32-BOOT]: Loading firmware images as raw binary\n");
        int bytes_loaded = load_image_mr(filename, program_mr);
        if (bytes_loaded < 0) {
            error_report("[AVR32-BOOT] Unable to load firmware image %s as raw binary",
                         firmware);
            return false;
        }
    }
    printf("[AVR32-BOOT] Loaded boot image successfully\n");

    return true;
}