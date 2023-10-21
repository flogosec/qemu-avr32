/*
 * QEMU AVR Disas
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
#include "cpu.h"

typedef struct DisasContext {
    // Provided by QEMU
    disassemble_info *dis;

    uint32_t addr;
    uint32_t pc;

    uint8_t bytes[4];
} DisasContext;

// Decode helper required only if insn wide is variable
static uint32_t decode_insn_load_bytes(DisasContext *ctx, uint32_t insn,
                                       int i, int n){
    printf("[AVR32-DISAS] decode_insn_load_bytes at: 0x%04x\n", ctx->pc);

    // TODO: We can probably just load 4 bytes and decide to only use 2 of them
    ctx->dis->read_memory_func(ctx->addr, ctx->bytes, 2, ctx->dis);

    if((ctx->bytes[0] & AVR32_EXTENDED_INSTR_FORMAT_MASK_LE) == AVR32_EXTENDED_INSTR_FORMAT_MASK_LE)
    {
        ctx->dis->read_memory_func(ctx->addr, ctx->bytes, 4, ctx->dis);

        insn = bfd_getb32(ctx->bytes);
        ctx->addr += 4;
        printf("[AVR32-DISAS] decode_insn_load_bytes, loaded (long): 0x%04x\n", insn);

    } else {
        insn = bfd_getb16(ctx->bytes) << 16;
        ctx->addr += 2;
        printf("[AVR32-DISAS] decode_insn_load_bytes, loaded (short): 0x%04x\n", insn);
    }

    return insn;
}

/* Include the auto-generated decoder.  */
static uint32_t decode_insn_load(DisasContext *ctx);
static bool decode_insn(DisasContext *ctx, uint32_t insn);
#include "decode-insn.c.inc"


#define output(mnemonic, format, ...) \
    (pctx->dis->fprintf_func(pctx->dis->stream, "%-9s " format, \
                              mnemonic, ##__VA_ARGS__))

int avr32_print_insn(bfd_vma addr, disassemble_info *dis)
{
    printf("[AVR32-DISAS] avr32_print_insn\n");
    DisasContext ctx;
    uint32_t insn;
    int i;

    ctx.dis = dis;
    ctx.pc = ctx.addr = addr;
//    ctx.len = 0;

    insn = decode_insn_load(&ctx);
    if (!decode_insn(&ctx, insn)) {
        ctx.dis->fprintf_func(ctx.dis->stream, ".byte\t");
        for (i = 0; i < ctx.addr - addr; i++) {
            if (i > 0) {
                ctx.dis->fprintf_func(ctx.dis->stream, ",");
            }
            ctx.dis->fprintf_func(ctx.dis->stream, "0x%02x", insn >> 24);
            insn <<= 8;
        }
    }

    return ctx.addr - addr;
}


#define INSN(opcode, mnemonic, format, ...)                              \
static bool trans_##opcode(DisasContext *pctx, arg_##opcode * a)        \
{                                                                       \
    output(#mnemonic, format, ##__VA_ARGS__);                             \
    return true;                                                        \
}

#define REG(x) avr32_cpu_r_names[x]

INSN(ABS,    ABS,      "%s",                              REG(a->rd))
INSN(ACALL,    ACALL,      "%s",                          REG(a->disp))

INSN(ACR,    ACR,      "%s",                                REG(a->rd))
INSN(ADC,    ADC,      "%s, %s, %s",                        REG(a->rd), REG(a->rx), REG(a->ry))
INSN(ADD_f1,    ADD,   "%s, %s",                            REG(a->rd), REG(a->rs))
INSN(ADD_f2,    ADC,   "%s, %s, %s",                        REG(a->rd), REG(a->rx), REG(a->ry))

INSN(ADD_cond,         ADD_cond,  "%s, %s, [0x%04x], %s",   REG(a->rx), REG(a->ry), a->cond, REG(a->rd))
INSN(ADDABS,           ADDABS,      "%s, %s, %s",           REG(a->rd), REG(a->rx), REG(a->ry))
INSN(ADDHHW,     ADDHHW,    "%s, %s, %s, %s, %s",     REG(a->rd), REG(a->rx), REG(a->x), REG(a->y), REG(a->rd))

INSN(AND_f1,    AND,      "%s, %s",                         REG(a->rd), REG(a->rs))
INSN(AND_f2,    AND,      "%s, %s, %s, 02%04x",             REG(a->rd), REG(a->rx), REG(a->ry), a->sa5)
INSN(AND_f3,    AND,      "%s, %s, %s, 02%04x",             REG(a->rd), REG(a->rx), REG(a->ry), a->sa5)
INSN(AND_cond,  AND,       "%s, %s, [0x%04x], %s",          REG(a->rx), REG(a->ry), a->cond, REG(a->rd))
INSN(ANDH,      ANDH,      "%s, %d",                        REG(a->rd), a->coh)
INSN(ANDL,      ANDL,      "%s, %d",                        REG(a->rd), a->coh)
INSN(ANDN,      ANDN,      "%s, %s",                        REG(a->rd), REG(a->rs))

INSN(ASR_f1,    ASR,      "%s, %s, %s",                    REG(a->rd), REG(a->rx), REG(a->ry))
INSN(ASR_f2,     ASR,      "%s, 0x%04x, 0x%02x",            REG(a->rd), a->bp4, a->bp1)
INSN(ASR_f3,     ASR,      "%s, %s, 0x%02x",                REG(a->rd), REG(a->rs), a->sa5)

INSN(BFEXTS,    BFEXTS,    "%s, %s, [0x%04x], [0x%04x]",      REG(a->rd), REG(a->rs), a->bp5, a->w5)
INSN(BFEXTU,    BFEXTU,    "%s, %s, [0x%04x], [0x%04x]",      REG(a->rd), REG(a->rs), a->bp5, a->w5)
INSN(BFINS,     BFINS,    "%s, %s, [0x%04x], [0x%04x]",       REG(a->rd), REG(a->rs), a->bp5, a->w5)

INSN(BLD,   BLD,        "%s, 0x%04x",               REG(a->rd), a->bp5)

INSN(BR_f1,     BR,        "cond3{%d}, disp: [0x%04x]",     a->rd, (a->disp))
INSN(BR_f2,   BR,        "cond4{%d}, disp2: [0x%04x], disp1: [0x%02x], disp0: [0x%04x]",    a->cond, a->disp2, a->disp1, a->disp0)

INSN(BREAKPOINT, BRK, "BREAK");

INSN(BREV_r,    BREV,      "%s",                              REG(a->rd))

INSN(BST,   BST,        "%s, 0x%04x",               REG(a->rd), a->bp5)

INSN(CACHE,     CACHE,          "%s, %d, 0x%04x",                 REG(a->rp), a->op5, a->disp11)

INSN(CASTSH,    CASTS.H,       "%s",                             REG(a->rd))
INSN(CASTSB,    CASTS.B,       "%s",                             REG(a->rd))
INSN(CASTUH,    CASTU.H,       "%s",                             REG(a->rd))
INSN(CASTUB,    CASTU.B,       "%s",                             REG(a->rd))

INSN(CBR,    CBR,    "%s, 0x%02x, 0x%02x",                     REG(a->rd), a->bp4, a->bp1)

INSN(CLZ,    CLZ,       "%s, %s",                             REG(a->rd), REG(a->rs))

INSN(COM,    COM,       "%s",                             REG(a->rd))

INSN(COP,     COP,       "CP: %d",                              a->cp)

INSN(CPB,           CP.B,       "%s, %s",                       REG(a->rd), REG(a->rs))
INSN(CPH,     CP.H,       "%s, %s",                       REG(a->rd), REG(a->rs))
INSN(CPW_f1,     CP.W,       "%s, %s",                       REG(a->rd), REG(a->rs))
INSN(CPW_f2,    CP.W,       "%s, 0x%04x",                  REG(a->rd), a->imm6)
INSN(CPW_f3,    CP.W,       "%s",                         REG(a->rd))

INSN(CPC_f1,     CPC,        "%s, %s",                          REG(a->rd), REG(a->rs))
INSN(CPC_f2,     CPC,           "%s",                           REG(a->rd))

INSN(CSRF,              CSRF,     "0x%02x",                            (a->bp5))
INSN(CSRFCZ,            CSRFCZ,   "0x%02x",                            (a->bp5))

INSN(DIVS,              DIVS,        "%s, %s, %s",                REG(a->rd), REG(a->rx), REG(a->ry))
INSN(DIVU,              DIVU,        "%s, %s, %s",                REG(a->rd), REG(a->rx), REG(a->ry))

INSN(EOR_f1,         EOR,          "%s, %s",                    REG(a->rd), REG(a->rs))
INSN(EOR_f2,         EOR,          "%s, %s, %s",                REG(a->rd), REG(a->rx), REG(a->ry))
INSN(EOR_f3,         EOR,          "%s, %s, %s",                REG(a->rd), REG(a->rx), REG(a->ry))
INSN(EOR_cond,    EOR,       "%s, %s, %s, %d",            REG(a->rd), REG(a->rx), REG(a->ry), a->cond)
INSN(EORH,              EORH,       "%s, 0x%04x",                    REG(a->rd), a->imm16)
INSN(EORL,              EORL,       "%s, 0x%04x",                    REG(a->rd), a->imm16)

INSN(FRS,               FRS, "FRS"                             );

INSN(ICALL,    ICALL,       "%s",                             REG(a->rd))

INSN(LDD_f1,   LDD,                  "%s, %s[0x%04x]",           REG(a->rp), REG(a->rd), a->rd)
INSN(LDD_f2,   LDD,                  "%s, %s[0x%04x]",           REG(a->rp), REG(a->rd), a->rd)
INSN(LDD_f3,   LDD,                  "%s, %s[0x%04x]",           REG(a->rp), REG(a->rd), a->rd)
INSN(LDD_f4,   LDD,                  "%s, %s, 0x%04x",           REG(a->rp), REG(a->rs), a->disp16)
INSN(LDD_f5,   LDD,                  "%s, %s, %s",               REG(a->rd), REG(a->rx), REG(a->ry))


INSN(LDsb_f1,   LDSB,                "%s, %s[0x%04x]",           REG(a->rp), REG(a->rd), a->rd)
INSN(LDsb_f2,   LDSB,                "%s, %s[%s<<%d]",       REG(a->rd), REG(a->rx), REG(a->ry), a->sa)
INSN(LDsb_cond,             LDsb_cond,        "%s, %s, 0x%04x",           REG(a->rd), REG(a->rp), a->disp9)

INSN(LDub_f1,        LDUB,            "%s, %s",                  REG(a->rp), REG(a->rd))
INSN(LDub_f2,        LDUB,            "%s, %s",                  REG(a->rp), REG(a->rd))
INSN(LDub_f3,        LDUB,            "%s, %s, 0x%02x",                  REG(a->rp), REG(a->rd), a->disp3)
INSN(LDUB_f4,        LDUB,        "%s, %s, 0x%04x",           REG(a->rp), REG(a->rd), a->disp16)
INSN(LDub_f5,           LDUB,        "%s, %s, %s, %d",           REG(a->rd), REG(a->rx), REG(a->ry), a->sa)
INSN(LDUB_cond,         LDUBc,        "%s, %s, 0x%04x",           REG(a->rd), REG(a->rp), a->disp9)


INSN(LDSH_f1,       LDSH,        "%s, %s",                        REG(a->rp), REG(a->rd))
INSN(LDSH_f2,       LDSH,        "%s, %s",                        REG(a->rp), REG(a->rd))
INSN(LDSH_f3,       LDSH,        "%s, %s, 0x%02x",               REG(a->rp), REG(a->rd), a->disp3);
INSN(LDSH_f4,       LDSH,        "%s, %s, 0x%02x",               REG(a->rp), REG(a->rd), a->disp16);
INSN(LDSH_f5,       LDSH,        "%s, %s, %s, 0x%02x",               REG(a->rd), REG(a->rx), REG(a->ry), a->sa);
INSN(LDSH_cond,     LDSHc,           "%s, %s",                       REG(a->rp), REG(a->rd))
INSN(LDUH_f1,   LDUH,        "%s, %s",                              REG(a->rp), REG(a->rd))
INSN(LDUH_f2,   LDUH,        "%s, %s",                              REG(a->rp), REG(a->rd))
INSN(LDUH_f3,   LDUH,        "%s, %s, 0x%02x",                        REG(a->rp), REG(a->rd), a->disp3)
INSN(LDUH_f4,   LDUH,        "%s, %s, 0x%02x",                        REG(a->rp), REG(a->rd), a->disp16)
INSN(LDUH_f5,       LDUH,        "%s, %s, %s, %d",                    REG(a->rd), REG(a->rx), REG(a->ry), a->sa)
INSN(LDUH_cond,     LDUHc,      "%s, %s",                             REG(a->rp), REG(a->rd))

INSN(LDW_f1,            LDW,          "%s, %s",                        REG(a->rp), REG(a->rd))
INSN(LDW_f2,            LDW,          "%s, %s",                        REG(a->rp), REG(a->rd))
INSN(LDW_f3,            LDW,    "%s, %s, 0x%04x",                REG(a->rp), REG(a->rd), a->disp5)
INSN(LDW_f4,            LDW,        "%s, %s, 0x%04x",           REG(a->rp), REG(a->rd), a->disp16)
INSN(LDW_f5,            LDW,        "%s, %s, %s",                REG(a->rd), REG(a->rx), REG(a->ry))
INSN(LDW_f6,            LDW,        "%s, %s, %s",                REG(a->rd), REG(a->rx), REG(a->ry))
INSN(LDW_cond,          LDWc,       "%s, %s, 0x%04x, 0x%04x",    REG(a->rp), REG(a->rd), a->cond4, a->disp9)

INSN(LDDPC,      LDDPC,    "%s, PC[0x%04x]",                    REG(a->rd), a->disp << 2)
INSN(LDDSP,      LDDSP,    "%s, %d",                       REG(a->rd), a->disp << 2)

INSN(LDINSB,            LDINSB,     "%s, %s, 0x%02x, 0x%04x",   REG(a->rd), REG(a->rp), a->part, a->disp12)
INSN(LDINSH,            LDINSB,     "%s, %s, 0x%02x, 0x%04x",   REG(a->rd), REG(a->rp), a->part, a->disp12)

INSN(LDM,   LDM,                "%d, rp: %s, list: 0x%04x",             a->op, REG(a->rp), a->list)
INSN(LDMTS,   LDM,                "%d, rp: %s, list: 0x%04x",             a->op, REG(a->rp), a->list)

INSN(LDSWPSH, LDSWPSH,            "%s, %s, 0x%04x", REG(a->rd), REG(a->rp), a->disp12)
INSN(LDSWPUH, LDSWPUH,            "%s, %s, 0x%04x", REG(a->rd), REG(a->rp), a->disp12)
INSN(LDSWPW, LDSWPW,              "%s, %s, 0x%04x", REG(a->rd), REG(a->rp), a->disp12)


INSN(LSL_f1,    LSL,       "%s, %s, %s",                  REG(a->rd), REG(a->rx), REG(a->ry))
INSN(LSL_f2,          LSL,       "%s, %0x2x, %0x2x",            REG(a->rd), a->bp4, a->bp1)
INSN(LSL_f3,    LSL,       "%s, %s, %d",                  REG(a->rd), REG(a->rs), a->sa5)
INSN(LSR_f1,    LSL,       "%s, %s, %s",                  REG(a->rd), REG(a->rx), REG(a->ry))
INSN(LSR_f2,          LSL,       "%s, %0x2x, %0x2x",            REG(a->rd), a->bp4, a->bp1)
INSN(LSR_f3,    LSL,       "%s, %s, 0x%04x",                  REG(a->rd), REG(a->rs), a->sa5)

INSN(MAC,               MAC,       "%s, %s, %s",                     REG(a->rd), REG(a->rx), REG(a->ry))
INSN(MACHHD,            MACHHD,       "%s, %s, %s",                  REG(a->rd), REG(a->rx), REG(a->ry))
INSN(MACHHW,            MACHHW,       "%s, %s, %s",                  REG(a->rd), REG(a->rx), REG(a->ry))
INSN(MACSD,             MACSD,     "%s, %s, %s",                     REG(a->rd), REG(a->rx), REG(a->ry))
INSN(MACUD,             MACUd,     "%s, %s, %s",                     REG(a->rd), REG(a->rx), REG(a->ry))

INSN(MAX,                MAX,       "%s, %s, %s",                  REG(a->rd), REG(a->rx), REG(a->ry))

INSN(MCALL,              MCALL,        "%s, disp: [0x%04x]",     REG(a->rp), (a->disp))

INSN(MEMC,               MEMC,        "bp: 0x%02x, imm: [0x%04x]",     a->bp5, a->imm15)
INSN(MEMS_bp5_imm15,     MEMS,        "bp: 0x%02x, imm: [0x%04x]",     a->bp5, a->imm15)
INSN(MEMT_bp5_imm15,     MEMT,        "bp: 0x%02x, imm: [0x%04x]",     a->bp5, a->imm15)

INSN(MFSR,        MFSR,                "%s, SysReg: [0x%04x]",    REG(a->rd), (a->sr))

INSN(MIN,           MIN,       "%s, %s, %s",                REG(a->rd), REG(a->rx), REG(a->ry))

INSN(MOV_f1,    MOV,     "%s, %d",                                 REG(a->rd), a->imm8)
INSN(MOV_f2,    MOV,     "%s, %d, %d, %d",                REG(a->rd), a->immu, a->immm, a->imml)
INSN(MOV_f3,    MOV,     "%s, %s",                           REG(a->rd), REG(a->rs))
INSN(MOVc_f1 ,    MOV,     "%s, %s, %d",                        REG(a->rd), REG(a->rs), a->cond4)
INSN(MOVc_f2,    MOV,     "%s, %d, %d",                         REG(a->rd), a->imm8, a->cond4)

INSN(MOVH,    MOVH,     "%s, 0x%04x",                           REG(a->rd), a->imm16)
INSN(MTDR,    MTDR,     "%s, 0x%04x",                           REG(a->rs), a->addr)

INSN(MTSR,    MTSR,     "%s, %s",                         REG(a->sr), REG(a->rs))

INSN(MUL_f1,   MUL,        "%s, %s",                         REG(a->rs), REG(a->rd))
INSN(MUL_f2, MUL,       "%s, %s, %s",                     REG(a->rd), REG(a->rx), REG(a->ry))
INSN(MUL_f3, MUL,       "%s, %s, 0x%04x",               REG(a->rd), REG(a->rs), a->imm8)
INSN(MULHHW, MULHHW,       "%s, %s, %s",                     REG(a->rd), REG(a->rx), REG(a->ry))
INSN(MULUD, MUL,       "%s, %s, %s",                     REG(a->rd), REG(a->rx), REG(a->ry))

INSN(MUSFR,      MUSFR,        "%s",                            REG(a->rs))
INSN(MUSTR,      MUSTR,        "%s",                            REG(a->rd))

INSN(NEG,   NEG,        "%s",                                REG(a->rd))
INSN(NOP,   NOP,        "NOP: %d",                                12)

INSN(OR_f1,   OR,           "%s, %s",                           REG(a->rs), REG(a->rd))
INSN(OR_f2,   OR,           "%s, %s, %s, 0x%04x",               REG(a->rd), REG(a->rx),REG(a->ry), a->sa5)
INSN(OR_f3,   OR,           "%s, %s, %s, 0x%04x",               REG(a->rd), REG(a->rx),REG(a->ry), a->sa5)
INSN(OR_cond, OR,           "%s, %s, %s, 0x%x",                 REG(a->rd), REG(a->rx),REG(a->ry), a->cond4)
INSN(ORH,   ORH,        "%s, 0x%04x",                           REG(a->rd), a->imm16)
INSN(ORL,   ORL,        "%s, 0x%04x",                           REG(a->rd), a->imm16)


INSN(POPM,   POPM,        "0x%04x",                           a->list)
INSN(PUSHM,   PUSHM,        "0x%04x",                           a->list)

INSN(RCALL_f1,    RCALL,     "0x%04x, 0x%02x",                  a->disp8, a->disp2)
INSN(RCALL_f2,    RCALL,     "0x%04x, 0x%02x, 0x%02x",          a->immu, a->immm, a->imml)

INSN(RET,   RET,        "%s, %d",                               REG(a->rd), a->cond4)
INSN(RETE,   RETE,        "RETE")
INSN(RETS,   RETS,        "RETS")

INSN(RJMP,    RJMP,     "0x%04x, 0x%02x",                       a->disp8, a->disp2)

INSN(ROL,   ROL,        "%s",                                REG(a->rd))
INSN(ROR,   ROR,        "%s",                                REG(a->rd))

INSN(RSUB_f1,   RSUB,        "%s, %s",                          REG(a->rd), REG(a->rs))
INSN(RSUB_f2,   RSUB,        "%s, %s, 0x%04x",          REG(a->rd), REG(a->rs), a->imm8)
INSN(RSUBc,   RSUBc,        "%s, %d",                          REG(a->rd), a->imm8)

INSN(SATU,   SATU,       "%s, bp5: 0x%04x, sa5: 0x%02x",         REG(a->rd), a->bp5, a->sa5)

INSN(SBC,   SBC,        "%s, %s, %s",                           REG(a->rd), REG(a->rx), REG(a->ry))
INSN(SBR,   SBR,        "%s, bp4: 0x%04x, bp1: 0x%02x",         REG(a->rd), a->bp4, a->bp1)

INSN(SCALL, SCALL,      "SCALL")
INSN(SCR,   SCR,        "%s",                                   REG(a->rd))

INSN(SLEEP, SLEEP,      "0x%02x,",                               a->op8)

INSN(SR,   SR,        "%s, cond4: 0x%04x",                      REG(a->rd), a->cond4)

INSN(SSRF,   SR,        "bp5: 0x%04x",                          a->bp5)

INSN(STB_f1,         STB,         "%s, %s",                  REG(a->rp), REG(a->rs))
INSN(STB_f2,            STB,         "%s, %s",                  REG(a->rp), REG(a->rs))
INSN(STB_f3,            STB,         "%s, %s, 0x%02x",          REG(a->rp), REG(a->rd), a->disp3)
INSN(STB_f4,            STB,         "%s, %s, 0x%04x",          REG(a->rp), REG(a->rs), a->imm16)
INSN(STB_f5,            STB,         "%s, %s, %s, 0x%02x",      REG(a->rd), REG(a->rx), REG(a->ry), a->sa)
INSN(STBc,              STB,         "%s, %s",                  REG(a->rp), REG(a->rd))
INSN(STD_f1,            STD,         "%s, %s",                  REG(a->rp), REG(a->rs))
INSN(STD_f2,            STD,         "%s, %s",                  REG(a->rp), REG(a->rs))
INSN(STD_f3,            STD,         "%s, %s",                  REG(a->rp), REG(a->rs))
INSN(STD_f4,            STD,          "%s, %s, 0x%04x",           REG(a->rp), REG(a->rs), a->disp16)
INSN(STDSP,  STD,                   "%s, 0x%04x",               REG(a->rd), a->disp)

INSN(STH_f1,   STH,                "%s, %s",                        REG(a->rp), REG(a->rs))
INSN(STH_f2,   STH,                "%s, %s",                        REG(a->rp), REG(a->rs))
INSN(STH_f3,   STH,                "%s, %s, 0x%02x",                REG(a->rp), REG(a->rd), a->disp3)
INSN(STH_f4,   STH,                "%s, %s, 0x%04x",                REG(a->rp), REG(a->rs), a->imm16)
INSN(STH_f5,   STH,                "%s, %s, %s, 0x%02x",            REG(a->rd), REG(a->rx), REG(a->ry), a->sa)
INSN(STHc,     STH,                "%s, %s",                        REG(a->rp), REG(a->rd))

INSN(STM,   STM,                "%d, rp: %s, list: 0x%04x",         a->op, REG(a->rp), a->list)
INSN(STW_f3, STW,               "%s, %s, 0x%04x",                   REG(a->rp), REG(a->rs), a->disp4)
INSN(STW_f4, STW,               "%s, %s, 0x%04x",                   REG(a->rp), REG(a->rs), a->imm16)
INSN(STW_f5, STW,               "%s, %s, %s",                       REG(a->rx), REG(a->ry), REG(a->rd))
INSN(STW_f2, STW,               "%s, %s",                           REG(a->rp), REG(a->rs))
INSN(STW_f1, STW,               "%s, %s",                           REG(a->rp), REG(a->rs))
INSN(STWcond, STWc,             "%s, %s, %d, 0x%04x",                           REG(a->rd), REG(a->rp), a->cond4, a->disp9)

INSN(SUB_f1,        SUB,        "%s, %s",                           REG(a->rs), REG(a->rd))
INSN(SUB_f2,        SUB,        "%s",                               REG(a->rd))
INSN(SUB_f3,        SUB,        "%s, 0x%0x4",                       REG(a->rd), a->imm8)
INSN(SUB_f4,        SUB,        "%s",                               REG(a->rd))
INSN(SUB_f5,        SUB,        "%s, %s, 0x%0x4",                   REG(a->rs), REG(a->rd), a->imm16)
INSN(SUBc_f1,       SUBc,       "%s, %d, %d, %0x2",                 REG(a->rd), a->f, a->cond4, a->imm8)
INSN(SUBc_f2,       SUBc,       "%s, %s, %s",                       REG(a->rd), REG(a->rx), REG(a->ry))

INSN(TNBZ,   TNBZ,        "%s",                             REG(a->rd))
INSN(TST,   TST,        "%s, %s",                           REG(a->rs), REG(a->rd))