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
DEF_HELPER_1(raise_illegal_instruction, noreturn, env)
DEF_HELPER_1(debug, noreturn, env)
DEF_HELPER_1(break, noreturn, env)
DEF_HELPER_4(macsathhw, void, env, i32, i32, i32)

#ifndef QEMU_AVR32_HELPER
#define QEMU_AVR32_HELPER
#include "tcg/tcg.h"
#define sflagC 0
#define sflagZ 1
#define sflagN 2
#define sflagV 3
#define sflagQ 4
#define sflagL 5
#define sflagT 14
#define sflagR 15
#define sflagGM 16
#define sflagEM 21


#endif //QEMU_AVR32_HELPER
