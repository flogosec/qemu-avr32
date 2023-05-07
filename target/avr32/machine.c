/*
 * QEMU AVR CPU
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
#include "migration/cpu.h"

static int get_sreg(QEMUFile *f, void *opaque, size_t size,
                    const VMStateField *field)
{
    uint32_t sreg;

    sreg = qemu_get_be32(f);
    return sreg;
}

static int put_sreg(QEMUFile *f, void *opaque, size_t size,
                    const VMStateField *field, JSONWriter *vmdesc)
{
    CPUAVR32AState *env = opaque;
    uint32_t sreg = env->sr;

    qemu_put_be32(f, sreg);
    return 0;
}

static const VMStateInfo vms_sreg = {
        .name = "sreg",
        .get = get_sreg,
        .put = put_sreg,
};

const VMStateDescription vms_avr32_cpu = {
        .name = "cpu",
        .version_id = 1,
        .minimum_version_id = 1,
        .fields = (VMStateField[]) {

                VMSTATE_UINT32_ARRAY(env.r, AVR32ACPU, AVR32A_REG_PAGE_SIZE),

                VMSTATE_SINGLE(env.sr, AVR32ACPU , 0, vms_sreg, uint32_t),

                VMSTATE_END_OF_LIST()
        }
};
