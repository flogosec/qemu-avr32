#ifndef QEMU_AVR32_AT32UC3_TIMER_H
#define QEMU_AVR32_AT32UC3_TIMER_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/irq.h"
#include "qemu/timer.h"
#include "hw/ptimer.h"

#define TYPE_AT32UC3_TIMER "at32uc3.timer"
OBJECT_DECLARE_SIMPLE_TYPE(AT32UC3TIMERState, AT32UC3_TIMER)


struct AT32UC3TimerChannel {
    uint32_t cmr;
    uint32_t ra, rb, rc;
    uint32_t sr;
    uint32_t imr;

    ptimer_state *timer;
};

typedef struct AT32UC3TimerChannel AT32UC3TimerChannel;

struct AT32UC3TIMERState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq irq;

    AT32UC3TimerChannel channels[3];
};

void startTimer(void);

#endif //QEMU_AVR32_AT32UC3_TIMER_H
