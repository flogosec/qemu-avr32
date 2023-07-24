/*
 * QEMU AVR CPU
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
#include "helper_conditions.h"
#include "tcg/tcg.h"

/*
 * Returns the value that needs to be matched by the return register
 */
int checkCondition(int condition, TCGv returnReg, TCGv cpu_r[], TCGv cpu_sflags[]){
    int val;
    switch (condition) {
        case 0x0:
            // equal
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagZ]);
            val = 1;
            break;
        case 0x1:
            //not equal
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagZ]);
            val = 0;
            break;
        case 0x02:
            // clear carry, higher (cc/hs).
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagC]);
            val = 0;
            break;
        case 0x03:
            // set carry, lower (cs/lo)
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagC]);
            val = 1;
            break;
        case 0x04:
            // set carry, lower (cs/lo)
            tcg_gen_setcond_i32(TCG_COND_EQ, returnReg, cpu_sflags[sflagN], cpu_sflags[sflagV]);
            val = 1;
            break;
        case 0x5:
            //lt
            tcg_gen_xor_i32(returnReg, cpu_sflags[sflagN], cpu_sflags[sflagV]);
            val = 1;
            break;
        case 0x6:
            //mi
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagN]);
            val = 1;
            break;
        case 0x07:
            // positiv, not slfagN
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagN]);
            val = 0;
            break;
        case 0x8:
            //ls
            tcg_gen_or_i32(returnReg, cpu_sflags[sflagC], cpu_sflags[sflagZ]);
            val = 1;
            break;
        case 0x9:
            //gt
            tcg_gen_setcond_i32(TCG_COND_EQ, returnReg, cpu_sflags[sflagN], cpu_sflags[sflagV]);
            tcg_gen_andc_i32(returnReg, returnReg, cpu_sflags[sflagZ]);
            val = 1;
            break;
        case 0xa:
            //le
            tcg_gen_xor_i32(returnReg, cpu_sflags[sflagN], cpu_sflags[sflagV]);
            tcg_gen_or_i32(returnReg, cpu_sflags[sflagZ], returnReg);
            val = 1;
            break;
        case 0x0b:
            // unsigned higher, HI, not C and not Z
            tcg_gen_not_i32(returnReg, cpu_sflags[sflagC]);
            tcg_gen_andc_i32(returnReg, returnReg, cpu_sflags[sflagZ]);
            tcg_gen_andi_i32(returnReg, returnReg, 0x00000001);
            val = 1;
            break;
        case 0xc:
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagV]);
            val = 1;
            break;
        case 0xd:
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagV]);
            val = 0;
            break;
        case 0xe:
            tcg_gen_mov_i32(returnReg, cpu_sflags[sflagQ]);
            val = 1;
            break;
        case 0x0f:
            tcg_gen_movi_i32(returnReg, 0x1);
            val = 1;
            break;
        default:
            printf("[COND] ERROR: undefined condition %d\n", condition);
            g_assert_not_reached();
    }
    return val;
}

void set_v_flag_add(TCGv op1, TCGv op2, TCGv result, TCGv cpu_sflags[]){
    TCGv temp = tcg_temp_new_i32();
    TCGv left = tcg_temp_new_i32();
    TCGv right = tcg_temp_new_i32();

    tcg_gen_and_i32(left, op1, op2);
    tcg_gen_andc_i32(left, left, result);
    tcg_gen_andc_i32(temp, result, op2);
    tcg_gen_andc_i32(right, temp, op1);
    tcg_gen_or_i32(cpu_sflags[sflagV], left, right);

    //tcg_temp_new_i32(temp);
    //tcg_temp_new_i32(right);
    //tcg_temp_new_i32(left);
}

void set_c_flag_add(TCGv op1, TCGv op2, TCGv result, TCGv cpu_sflags[]){
    TCGv temp = tcg_temp_new_i32();
    TCGv left = tcg_temp_new_i32();
    TCGv right = tcg_temp_new_i32();

    tcg_gen_and_i32(left, op1, op2);
    tcg_gen_andc_i32(temp, op1, result);
    tcg_gen_or_i32(left, left, temp);
    tcg_gen_andc_i32(right, op2, result);
    tcg_gen_or_i32(cpu_sflags[sflagC], left, right);
}

void set_v_flag_cp(TCGv op1, TCGv op2, TCGv result, TCGv cpu_sflags[]){
    TCGv left = tcg_temp_new_i32();
    TCGv right = tcg_temp_new_i32();

    tcg_gen_andc_i32(left, op1, op2);
    tcg_gen_andc_i32(left, left, result);
    tcg_gen_andc_i32(right, op2, op1);
    tcg_gen_and_i32(right, right, result);
    tcg_gen_or_i32(cpu_sflags[sflagV], left, right);
}

void set_c_flag_cp(TCGv op1, TCGv op2, TCGv result, TCGv cpu_sflags[]){
    TCGv left = tcg_temp_new_i32();
    TCGv right = tcg_temp_new_i32();

    tcg_gen_andc_i32(left, op2, op1);
    tcg_gen_and_i32(right,  op2, result);
    tcg_gen_or_i32(left, left, right);
    tcg_gen_andc_i32(right, result, op1);
    tcg_gen_or_i32(cpu_sflags[sflagC], left, right);
}

void set_flags_cpc(TCGv rd, TCGv rs, TCGv res, TCGv cpu_sflags[]){
    TCGv temp = tcg_temp_new_i32();
    // Z-flag
    tcg_gen_setcondi_i32(TCG_COND_EQ, temp, res, 0);
    tcg_gen_and_i32(cpu_sflags[sflagZ], temp, cpu_sflags[sflagZ]);

    //Prepare bit[31]
    tcg_gen_shri_i32(res, res, 31);
    tcg_gen_shri_i32(rd, rd, 31);
    tcg_gen_shri_i32(rs, rs, 31);

    // N-flag
    tcg_gen_mov_i32(cpu_sflags[sflagN], res);

    //c and v flag
    set_c_flag_cp(rd, rs, res, cpu_sflags);
    set_v_flag_cp(rd, rs, res, cpu_sflags);
}

void cpw_instruction(TCGv Rd, TCGv Rs, TCGv cpu_sflags[]){
    TCGv res = tcg_temp_new_i32();

    tcg_gen_sub_i32(res, Rd, Rs);

    // set N flag: N ← RES[31]
    tcg_gen_shri_i32(cpu_sflags[sflagN], res, 31);

    // set Z flag: Z ← (RES[31:0] == 0)
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_sflags[sflagZ], res, 0); /* Zf = R == 0 */


    //move bit 31 to position 0
    tcg_gen_shri_i32(res, res, 31);
    tcg_gen_shri_i32(Rd, Rd, 31);
    tcg_gen_shri_i32(Rs, Rs, 31);

    //Set V flag
    set_v_flag_cp(Rd, Rs, res, cpu_sflags);

    //Set C flag
    set_c_flag_cp(Rd, Rs, res, cpu_sflags);

    //tcg_temp_new_i32(res);
    return;
}