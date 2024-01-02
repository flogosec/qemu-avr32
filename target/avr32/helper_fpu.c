/*
 * QEMU AVR32 CPU FPU Helper
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
#include "cpu.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "exec/helper-proto.h"
#include "fpu/softfloat.h"


static void fmacs(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry, uint32_t ra){
    float prod = float32_mul(env->r[rx], env->r[ry], &env->fp_status);
    float sum = float32_add(env->r[ra], prod, &env->fp_status);
    env->r[rd] = sum;
}

static void fnmacs(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry, uint32_t ra){
    float prod = float32_mul(env->r[rx], env->r[ry], &env->fp_status);
    float sum = float32_add(env->r[ra], prod, &env->fp_status);
    // -1 = 0xbf800000
    prod = float32_mul(sum, 0xbf800000, &env->fp_status);
    env->r[rd] = prod;
}

static void fmscs(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry, uint32_t ra){
    float prod = float32_mul(env->r[rx], env->r[ry], &env->fp_status);
    float sum = float32_sub(env->r[ra], prod, &env->fp_status);
    env->r[rd] = sum;
}

static void fnmscs(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry, uint32_t ra){
    float prod = float32_mul(env->r[rx], env->r[ry], &env->fp_status);
    float sum = float32_sub(env->r[ra], prod, &env->fp_status);
    prod = float32_mul(sum, 0xbf800000, &env->fp_status);
    env->r[rd] = prod;
}

static void fadds(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry){
    env->r[rd] = float32_add(env->r[rx], env->r[ry], &env->fp_status);
}

static void fsubs(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry){
    env->r[rd] = float32_sub(env->r[rx], env->r[ry], &env->fp_status);
}

static void fmuls(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry){
    env->r[rd] = float32_mul(env->r[rx], env->r[ry], &env->fp_status);
}

static void fnmuls(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry){
    float prod = float32_mul(env->r[rx], env->r[ry], &env->fp_status);
    prod = float32_mul(prod, 0xbf800000, &env->fp_status);
    env->r[rd] = prod;
}

static void fcastsws(CPUAVR32AState *env, uint32_t rd, uint32_t rx){
    env->r[rd] = int32_to_float32(env->r[rx], &env->fp_status);
}

static void fcastuws(CPUAVR32AState *env, uint32_t rd, uint32_t rx){
    env->r[rd] = uint32_to_float32(env->r[rx], &env->fp_status);
}

static void fcastrssw(CPUAVR32AState *env, uint32_t rd, uint32_t rx){
    env->r[rd] = float32_to_int32(env->r[rx], &env->fp_status);
}

static void fcastrsuw(CPUAVR32AState *env, uint32_t rd, uint32_t rx){
    env->r[rd] = float32_to_uint32(env->r[rx], &env->fp_status);
}

static void fcps(CPUAVR32AState *env, uint32_t rx, uint32_t ry){
    int32_t res = float32_unordered(env->r[rx], env->r[ry], &env->fp_status);
    if(res){
        env->sflags[sflagC] = 0;
        env->sflags[sflagN] = 0;
        env->sflags[sflagV] = 1;
        env->sflags[sflagZ] = 0;
        return;
    }

    res = float32_compare(env->r[rx], env->r[ry], &env->fp_status);

    if(res < 0){
        env->sflags[sflagC] = 1;
        env->sflags[sflagN] = 1;
        env->sflags[sflagV] = 0;
        env->sflags[sflagZ] = 0;
        return;
    }
    else if(res > 0){
        env->sflags[sflagC] = 0;
        env->sflags[sflagN] = 0;
        env->sflags[sflagV] = 0;
        env->sflags[sflagZ] = 0;
    }
    else if(res == 0){
        env->sflags[sflagC] = 0;
        env->sflags[sflagN] = 0;
        env->sflags[sflagV] = 0;
        env->sflags[sflagZ] = 1;
    }
}

static void fchks(CPUAVR32AState *env, uint32_t ry){
    if(env->r[ry] == 0x7FC00000){
        env->sflags[sflagC] = 1;
        env->sflags[sflagN] = 1;
        env->sflags[sflagV] = 0;
        env->sflags[sflagZ] = 0;
    }
    else if(float32_is_infinity(env->r[ry])){
        env->sflags[sflagC] = 0;
        env->sflags[sflagN] = 0;
        env->sflags[sflagV] = 0;
        env->sflags[sflagZ] = 0;
    }
    else if(float32_is_denormal(env->r[ry])){
        //TODO add exponent check
        env->sflags[sflagC] = 0;
        env->sflags[sflagN] = 0;
        env->sflags[sflagV] = 0;
        env->sflags[sflagZ] = 1;
    }
    else if(float32_is_normal(env->r[ry])){
        env->sflags[sflagC] = 0;
        env->sflags[sflagN] = 0;
        env->sflags[sflagV] = 1;
        env->sflags[sflagZ] = 0;
    }
}

void helper_cop(CPUAVR32AState *env, uint32_t rd, uint32_t rx,  uint32_t ry, uint32_t op)
{
    // opm is used as RA register
    if(!(op >> 6)){
        uint32_t ra_num = (op & 0b0011110) >> 1;
        op = (op & 0b1100001);
        switch (op) {
            case 0:
                fmacs(env, rd, rx, ry, ra_num);
                break;
            case 1:
                fnmacs(env, rd, rx, ry, ra_num);
                break;
            case 0x20:
                fmscs(env, rd, rx, ry, ra_num);
                break;
            case 0x21:
                fnmscs(env, rd, rx, ry, ra_num);
                break;
        }

    }
    //opm is a value
    else{
        if(op == 0b1000000){
            fadds(env, rd, rx, ry);
        }
        else if(op == 0b1000010){
            fsubs(env, rd, rx, ry);
        }
        else if(op == 0b1000100){
            fmuls(env, rd, rx, ry);
        }
        else if(op == 0b1000110){
            fnmuls(env, rd, rx, ry);
        }
        else if((op & 0b1111000) == 0b1001000){
            if(op == 0b1001100){
                fcastsws(env, rd, rx);
            }
            else{
                fcastuws(env, rd, rx);
            }
        }
        else if((op & 0b1111010) == 0b1010010){
            if(op == 0b1010110){
                fcastrssw(env, rd, rx);
            }
            else{
                fcastrsuw(env, rd, rx);
            }
        }
        else if((op & 0b1111000) == 0b1011000){
            fcps(env, rx, ry);
        }
        else if((op == 0b1011010)){
            fchks(env, ry);
        }

    }
}