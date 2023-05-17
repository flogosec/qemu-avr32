#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/timer/at32uc3_timer.h"
#include "migration/vmstate.h"
#include "qemu/timer.h"
#include "sys/time.h"
#include "qom/object.h"
#include "qemu/main-loop.h"
#include "threads.h"

#define TIMER_CH_CCR 0x00
#define TIMER_CH_CMR 0x04
#define TIMER_CH_CV  0x10
#define TIMER_CH_RA  0x14
#define TIMER_CH_RB  0x18
#define TIMER_CH_RC  0x1C
#define TIMER_CH_SR  0x20
#define TIMER_CH_IER 0x24
#define TIMER_CH_IDR 0x28
#define TIMER_CH_IMR 0x2c

#define TIMER_BCR 0xC0
#define TIMER_FEAT 0xF8
#define TIMER_VERSION 0xFC


#define TIMER_CH_CCR_CLKEN  1 << 0
#define TIMER_CH_CCR_CLKDIS 1 << 1
#define TIMER_CH_CCR_SWTRG  1 << 2

#define TIMER_CH_TCCLKS 0b111 << 0
#define TIMER_CH_WAVSEL 0b11 << 13
#define TIMER_CH_WAVE   1 << 15

#define TIMER_CH_SR_CLKSTA 1 << 16

#define TIMER_BCR_SYNC 1 << 0

//static bool timerRunning = false;
pthread_cond_t timerWait = PTHREAD_COND_INITIALIZER;
pthread_mutex_t timerLock = PTHREAD_MUTEX_INITIALIZER;


static void at32uc3_timer_reset(DeviceState *dev)
{
    AT32UC3TIMERState *s = AT32UC3_TIMER(dev);

    for(int i = 0; i < 3; ++i) {
        s->channels[i].cmr = 0x0;
        s->channels[i].ra = 0x0;
        s->channels[i].rb = 0x0;
        s->channels[i].rc = 0x0;
        s->channels[i].sr = 0x0;
        s->channels[i].imr = 0x0;
    }
}

static void timer_update_irq(AT32UC3TIMERState* s, int channel_idx)
{
    // TODO: The IMR is missing, the simulation breaks, when using IMR, this shouldn't happen
    if(s->channels[channel_idx].sr & 0xff)  { //  & s->channels[channel_idx].imr
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static bool channel_is_enabled(AT32UC3TimerChannel* ch)
{
    return ch->sr & TIMER_CH_SR_CLKSTA;
}

static uint8_t channel_get_wavsel(AT32UC3TimerChannel* ch)
{
    return (ch->cmr & TIMER_CH_WAVSEL) >> 13;
}

static uint64_t at32uc_timer_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3TIMERState* s = AT32UC3_TIMER(opaque);

    if(addr < TIMER_BCR) {
        uint32_t ret_val;
        int channel_idx = addr / 0x40;
        uint32_t channel_offset = addr % 0x40;
        AT32UC3TimerChannel* ch = &s->channels[channel_idx];

        switch(channel_offset) {
            case TIMER_CH_CCR: {
                printf("[at32uc_timer_read] TIMER_CH_CCR is write-only\n");
                return 0xdeadbeef;
            }
            case TIMER_CH_CMR: {
                printf("[at32uc_timer_read] TIMER_CH_CMR\n");
                return ch->cmr;
            }
            case TIMER_CH_CV: {
                if(channel_is_enabled(ch)) {
                    uint16_t max_count = 0xffff;

                    if(channel_get_wavsel(ch) == 2) {
                        max_count = ch->rc;
                    }

                    // The ptimer counts down, but the doc specifies that our timer ticks up
                    return max_count - ptimer_get_count(ch->timer);
                } else {
                    return 0;
                }
            }
            case TIMER_CH_RA: {
                return ch->ra;
            }
            case TIMER_CH_RB: {
                return ch->rb;
            }
            case TIMER_CH_RC: {
                return ch->rc;
            }
            case TIMER_CH_SR: {
//                printf("[at32uc_timer_read] TIMER_CH_SR\n");
                ret_val = ch->sr;

                // The interrupt status registers are all cleared when the SR is read:
                ch->sr &= ~0b11111111;
                timer_update_irq(s, channel_idx);

                return ret_val;
            }

            case TIMER_CH_IER:
            case TIMER_CH_IDR:
            {
                printf("[at32uc_timer_read] TIMER_CH_IE/DR is write-only\n");
                return 0xdeadbeef;
            }
            case TIMER_CH_IMR: {
                return ch->imr;
            }
            default: {
                printf("[at32uc_timer_write] addr=0x%lx is not implemented!\n", addr);
                exit(-1);
                return 0xdeadbeef;
            }
        }
    } else {
        printf("[at32uc_timer_write] addr=0x%lx is not implemented!\n", addr);
        exit(-1);
        return 0x0;
    }
}

static void at32uc_timer_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    AT32UC3TIMERState* s = opaque;

    printf("[at32uc_timer_write] addr=0x%lx val=0x%lx\n", addr, val64);

    if(addr < TIMER_BCR) {
        uint32_t mask;
        int channel_idx = addr / 0x40;
        uint32_t channel_offset = addr % 0x40;
        AT32UC3TimerChannel *ch = &s->channels[channel_idx];

        switch(channel_offset) {
            case TIMER_CH_CCR: {
                ptimer_transaction_begin(ch->timer);
                if(ch->sr & TIMER_CH_SR_CLKSTA) {
                    ptimer_stop(ch->timer);
                }

                if(val64 & TIMER_CH_CCR_CLKEN && (val64 & TIMER_CH_CCR_CLKDIS) == 0) {
                    // Writing a one to this bit will enable the clock if CLKDIS is not one.
                    // Writing a zero to this bit has no effect.
                    printf("[at32uc_timer_write] Enabled channel %d\n", channel_idx);
                    ch->sr |= TIMER_CH_SR_CLKSTA;
                }

                if(val64 & TIMER_CH_CCR_CLKDIS) {
                    // Writing a one to this bit will disable the clock.
                    // Writing a zero to this bit has no effect
                    ch->sr &= ~TIMER_CH_SR_CLKSTA;
                }

                if(val64 & TIMER_CH_CCR_SWTRG) {
                    // 1: Writing a one to this bit will perform a software trigger: the counter is reset and the clock is started.
                    // 0: Writing a zero to this bit has no effect.
                    // TODO

                    ptimer_set_limit(ch->timer, ptimer_get_limit(ch->timer), 0);
//                     s->timerBaseValue = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL_RT);
                }

                if(ch->sr & TIMER_CH_SR_CLKSTA) {
                    ptimer_run(ch->timer, 0);
                }

                ptimer_transaction_commit(ch->timer);
                break;
            }
            case TIMER_CH_CMR: {
                ch->cmr = val64;
                printf("[at32uc_timer_write] TIMER_CH_CMR - config=0x%x\n", ch->cmr);

                if(val64 & TIMER_CH_TCCLKS) {
                    printf("[at32uc_timer_write] TIMER_CH_CMR - Selected clock %d\n", ch->cmr & TIMER_CH_TCCLKS);
                }

                if(val64 & TIMER_CH_WAVE) {
                    printf("[at32uc_timer_write] TIMER_CH_CMR - Waveform mode is enabled\n");
                } else {
                    printf("[at32uc_timer_write] TIMER_CH_CMR - Capture mode is enabled\n");
                    printf("[at32uc_timer_write] TIMER_CH_CMR - Capture mode is not implemented!\n");
                    exit(0);
                }

                if(val64 & TIMER_CH_WAVSEL) {
                    uint8_t wavsel = channel_get_wavsel(ch);

                    printf("[at32uc_timer_write] TIMER_CH_CMR - TIMER_CH_WAVSEL=%d\n", wavsel);

                    if(wavsel == 2) {
                        ptimer_transaction_begin(ch->timer);
                        ptimer_set_limit(ch->timer, ch->rc, 0);
                        ptimer_transaction_commit(ch->timer);
                    } else {
                        printf("[at32uc_timer_write] TIMER_CH_CMR - WAVSEL != 2 is not implemented, WAVSEL=%d!\n", wavsel);
                    }
                }

                break;
            }

            case TIMER_CH_RA: {
                ch->ra = val64;
                break;
            }
            case TIMER_CH_RB: {
                ch->rb = val64;
                break;
            }
            case TIMER_CH_RC: {
                ch->rc = val64;
                uint8_t wavsel = (ch->cmr & TIMER_CH_WAVSEL) >> 13;

                if(wavsel == 2) {
                    ptimer_transaction_begin(ch->timer);
                    ptimer_set_limit(ch->timer, ch->rc, 0); // TODO: Not sure if reload=0 is correct here
                    ptimer_transaction_commit(ch->timer);
                }

                printf("[at32uc_timer_write] TIMER_CH_RC RC=0x%x\n", ch->rc);
                break;
            }
            case TIMER_CH_SR: {
                printf("[at32uc_timer_write] TIMER_CH_SR is read-only\n");
                break;
            }
            case TIMER_CH_IER: {
                mask = val64;
                ch->imr |= mask;
                printf("[at32uc_timer_write] TIMER_CH_IER IMR=0x%x, mask=0x%lx\n", ch->imr, val64);
                break;
            }
            case TIMER_CH_IDR: {
                mask = val64;
                ch->imr &= ~mask;
                printf("[at32uc_timer_write] TIMER_CH_IDR IMR=0x%x, mask=0x%lx\n", ch->imr, val64);
                break;
            }
            case TIMER_CH_IMR: {
                printf("[at32uc_timer_write] TIMER_CH_IMR is read-only\n");
                break;
            }
            default: {
                printf("[at32uc_timer_write] addr=0x%lx is not implemented!\n", addr);
                break;
            }
        }
    } else {
        switch(addr) {
            case TIMER_BCR: {
                if(val64 & TIMER_BCR_SYNC) {
                    // Writing a one to this bit asserts the SYNC signal which generates a software trigger simultaneously for each of the channels.
                    // Writing a zero to this bit has no effect.
                    // TODO
                }
                break;
            }
            default: {
                printf("[at32uc_timer_write] addr=0x%lx is not implemented!\n", addr);
            }
        }
    }
}

static const MemoryRegionOps timer_ops = {
        .read = at32uc_timer_read,
        .write = at32uc_timer_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

void startTimer(void){
//    timerRunning = true;
//    pthread_cond_signal(&timerWait);
}

//static void *timer_thread(void *opaque)
//{
//    AT32UC3TIMERState *tcs = opaque;
//
//    while (1) {
//        if(!timerRunning){
//            pthread_cond_wait(&timerWait, &timerLock);
//            usleep(200);
//        }
//
//        qemu_mutex_lock_iothread();
//        if(!timerRunning){
//            qemu_mutex_unlock_iothread();
//            continue;
//        }
//        qemu_set_irq(tcs->irq, 1);
//        qemu_mutex_unlock_iothread();
//        usleep(50);
//    }
//    return NULL;
//}

static void avr32_ch2_timer_tick(void* opaque)
{
    AT32UC3TIMERState* s = opaque;

    s->channels[2].sr |= 1;

//    printf("[avr32_ch0_timer_tick] IMR=0x%x\n", s->channels[2].imr);
    timer_update_irq(s, 2);
}

static void at32uc3_timer_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3TIMERState *s = AT32UC3_TIMER(dev);

    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->mmio, OBJECT(s), &timer_ops, s, TYPE_AT32UC3_TIMER, 0x200); // R_MAX * 4 = size of region
    sysbus_init_mmio(sbd, &s->mmio);

//    qemu_mutex_init(&s->thr_mutex);
//    qemu_cond_init(&s->thr_cond);
//    qemu_thread_create(&s->thread, "tc0_timer", timer_thread, s, QEMU_THREAD_JOINABLE);

    // TIMER_CLOCK 4 frequency for TC0:
    // Frequency: PBC clock / 32 (TIMER0_CLOCK4) (Section 31.8.1)
    // PBC Clock: f_PBx = f_main / 2 ^ (PBSEL + 1) (section 7.6.1.2)
    // f_main = ?
        // Between 0.4 and 20 MHz (Section 8.5.1)
    // PBSEL =>  Clock Select (PBxSEL) comes from a Power Manager (PM) register (Section 7.7.4)

    for(int i = 0; i < 3; ++i) {
        s->channels[i].timer = ptimer_init(avr32_ch2_timer_tick, s, PTIMER_POLICY_LEGACY);

        ptimer_transaction_begin(s->channels[i].timer);
        ptimer_set_freq(s->channels[i].timer, 100 * 1000);
        ptimer_set_limit(s->channels[i].timer, 0xffff, 1);
        ptimer_transaction_commit(s->channels[i].timer);
    }
}

static void at32uc3_timer_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_timer_realize;
    dc->reset = at32uc3_timer_reset;
}

static const TypeInfo at32uc3_timer_info = {
        .name           = TYPE_AT32UC3_TIMER,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3TIMERState),
        .class_init     = at32uc3_timer_class_init,
};

static void at32uc3_timer_register_types(void)
{
    type_register_static(&at32uc3_timer_info);
}

type_init(at32uc3_timer_register_types)