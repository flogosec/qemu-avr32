#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/ssi/at32uc3_spi.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"

// AT32UC SPI Reference: http://ww1.microchip.com/downloads/en/DeviceDoc/doc32117.pdf 26.8 (pg. 670f.)
#define AT32UC_SPI_CR   0x00
#define AT32UC_SPI_MR   0x04
#define AT32UC_SPI_RDR  0x08
#define AT32UC_SPI_TDR  0x0C
#define AT32UC_SPI_SR   0x10

#define AT32UC_SPI_CSR0   0x30
#define AT32UC_SPI_CSR1   0x34
#define AT32UC_SPI_CSR2   0x38
#define AT32UC_SPI_CSR3   0x3c

#define CR_SPIEN 1 << 0
#define CR_SWRST 1 << 7
#define TDR_PCS 0b1111 << 16
#define TDR_LASTXFER 1 << 24

#define MR_MSTR     1 << 0
#define MR_PS       1 << 1
#define MR_PCSDEC   1 << 2
#define MR_MODFDIS  1 << 4
#define MR_RXFIFOEN 1 << 6
#define MR_PCS      0b1111 << 16
#define GET_MR_PCS(mr) ((mr & MR_PCS) >> 16)

#define SR_RDRF 1 << 0 // Receive Data Register Full
#define SR_TDRE 1 << 1 // Transmit Data Register Empty
#define SR_TXEMPTY 1 << 9 // Transmission Registers Empty
#define SR_SPIENS 1 << 16

#define FIFO_CAPACITY  4


static void at32uc_spi_transfer(AT32UC3SPIState *s) {
    uint16_t tx = s->shift_reg;
    uint16_t rx = ssi_transfer(s->spi, tx);
//    printf("[at32uc_spi_transfer] tx=0x%x, rx=0x%x\n", tx, rx);

    // RDRF: Receive Data Register Full
    // Data has been received and the received data has been transferred from the serializer to RDR since the last read of RDR.
    s->spi_rdr = (uint32_t) rx;
    s->spi_sr |= (SR_TXEMPTY | SR_RDRF | SR_TDRE);
}

//static void rxfifo_reset(AT32UC3SPIState *spi)
//{
//    fifo32_reset(&spi->rx_fifo);
//}

static void at32uc_spi_update_cs(AT32UC3SPIState *s)
{
    int i;
    // TODO: The following 'only one line is selected'-mechanic is missing
    // When operating without decoding, the SPI makes sure that in any case only one chip select line
    //is activated, i.e. driven low at a time. If two bits are defined low in a PCS field, only the lowest
    // numbered chip select is driven low

    for (i = 0; i < s->num_cs; ++i) {
        qemu_set_irq(s->cs_lines[i], !((~GET_MR_PCS(s->spi_mr)) & (1 << i)));
    }
}

static void at32uc_spi_do_reset(AT32UC3SPIState* spi)
{
    spi->spi_cr = 0;
    spi->spi_mr = 0;
    spi->spi_tdr = 0;
    spi->spi_sr = 0;
    spi->spi_csr0 = 0;
    spi->spi_csr1 = 0;
    spi->spi_csr2 = 0;
    spi->spi_csr3 = 0;

//    rxfifo_reset(spi);
    at32uc_spi_update_cs(spi);
}

static uint64_t at32uc_spi_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3SPIState* s = opaque;

    switch(addr) {
        case AT32UC_SPI_CR:
            printf("[at32uc_spi_read] AT32UC_SPI_CR is write-only\n");
            return 0xdead;
        case AT32UC_SPI_MR:
//            printf("[at32uc_spi_read] AT32UC_SPI_MR, val: 0x%x\n", s->spi_mr);
            return s->spi_mr;
        case AT32UC_SPI_RDR:
//            (void) 0;
            s->spi_sr &= ~SR_RDRF;
            uint16_t rdr = s->spi_rdr;
            s->spi_rdr = 0xdead;
//            printf("[at32uc_spi_read] AT32UC_SPI_RDR, val: 0x%x\n", rdr);
            return rdr;
        case AT32UC_SPI_TDR:
            printf("[at32uc_spi_read] AT32UC_SPI_TDR is write-only\n");
            return 0xdead;
        case AT32UC_SPI_SR:
//            printf("[at32uc_spi_read] AT32UC_SPI_SR, val: 0x%x\n", s->spi_sr);
//            s->spi_sr |= SR_SPIENS | SR_TXEMPTY | SR_TDRE | SR_RDRF;
            return s->spi_sr;
        case AT32UC_SPI_CSR0:
            return s->spi_csr0;
        case AT32UC_SPI_CSR1:
            return s->spi_csr1;
        case AT32UC_SPI_CSR2:
            return s->spi_csr2;
        case AT32UC_SPI_CSR3:
            return s->spi_csr3;
        default:
            printf("[at32uc_spi_read] unknown, addr: 0x%lx\n", addr << 2);
            return 0;
    }
}

static void at32uc_spi_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
//    static int counter = 440; // Number to capture the first two fl512 read interactions to check for bad blocks
//    static int counter = 20000; // Until first [LoadFDIRConfigToFlash] Wrong configID value
    static int counter = 100000;
    counter--;
    AT32UC3SPIState* s = opaque;
    uint64_t val = val64;

    switch(addr) {
        case AT32UC_SPI_CR:
            if(val64 & CR_SPIEN) { // SPIEN: SPI Enable
                // Writing a one to this bit will enable the SPI to transfer and receive data.

                printf("[at32uc_spi_write] AT32UC_SPI_CR: CR_SPIEN\n");
                s->spi_cr |= CR_SPIEN;
                // TDRE equals zero when the SPI is disabled or at reset. The SPI enable command sets this bit to one.
                s->spi_sr |= (SR_SPIENS | SR_TDRE);
                printf("[at32uc_spi_write] AT32UC_SPI_CR, s->spi_sr=0x%x\n", s->spi_sr);
            } else {
                // Writing a zero to this bit has no effect
            }

            if(val64 & CR_SWRST){
                printf("[at32uc_spi_write] AT32UC_SPI_CR: CR_SWRST\n");
                at32uc_spi_do_reset(s);
            }
            break;
        case AT32UC_SPI_MR:
            s->spi_mr = val64;

            if(val64 & MR_MSTR) {
//                Master/Slave Mode
//                1: SPI is in master mode.
//                0: SPI is in slave mode
//                printf("[at32uc_spi_write] AT32UC_SPI_MR 0x%x MR_MSTR, Master/Slave Mode\n", MR_MSTR);
                val &= ~MR_MSTR;
            }

            if(val64 & MR_PS) {
                // PS: Peripheral Select
                // 1: Variable Peripheral Select.
                // 0: Fixed Peripheral Select.
                printf("[at32uc_spi_write] AT32UC_SPI_MR 0x%x MR_PS, Peripheral Select\n", MR_PS);
                val &= ~MR_PS;
            }

            if(val64 & MR_PCSDEC) {
                printf("[at32uc_spi_write] AT32UC_SPI_MR 0x%x MR_PCSDEC, Chip Select Decode, NOT IMPLEMENTED\n", MR_PCSDEC);
                val &= ~MR_PCSDEC;
            }

            if(val64 & MR_MODFDIS) {
                // Mode Fault Detection, Ignored
//                printf("[at32uc_spi_write] AT32UC_SPI_MR 0x%x MR_MODFDIS, Mode Fault Detection\n", MR_MODFDIS);
                val &= ~MR_MODFDIS;
            }

            if(val64 & MR_RXFIFOEN) {
                // RXFIFOEN: FIFO in Reception Enable
                printf("[at32uc_spi_write] AT32UC_SPI_MR 0x%x MR_RXFIFOEN, FIFO in Reception Enable\n", MR_RXFIFOEN);
                val &= ~MR_RXFIFOEN;
//                s->spi_mr |= MR_RXFIFOEN;
            }

            if(val64 & MR_PCS) {
                // Peripheral Chip Select
                val &= ~MR_PCS;
                at32uc_spi_update_cs(s);
            }

            if(val) {
                printf("[at32uc_spi_write] AT32UC_SPI_MR 0x%lx\n", val64);
            }
            break;
        case AT32UC_SPI_RDR: // Receive Data Register
            printf("[at32uc_spi_write] AT32UC_SPI_RDR is read-only\n");
            break;
        case AT32UC_SPI_TDR: // Transmit Data Register
            if(val64 & TDR_PCS) {
                printf("[at32uc_spi_write] AT32UC_SPI_TDR, TDR Peripheral Chip Select not implemented\n");
                break;
            }
            if(val64 & TDR_LASTXFER) {
                printf("[at32uc_spi_write] AT32UC_SPI_TDR, LASTXFER not implemented\n");
                break;
            }

            // If new data is written to TDR during the transfer, it stays in it until the current transfer is com-
            // pleted. Then, the received data is transferred from the Shift Register to RDR, the data in TDR is
            // loaded in the Shift Register and a new transfer starts.

            s->spi_sr &= ~SR_TXEMPTY;
            s->spi_tdr = (uint16_t) (val64 & 0xffff);

            s->shift_reg = s->spi_tdr;
            // TDRE
            // 1: This bit is set when the last data written in the TDR register has been transferred to the serializer.
            // 0: This bit is cleared when data has been written to TDR and not yet transferred to the serializer.
            s->spi_sr |= SR_TDRE;

//            printf("[at32uc_spi_write] AT32UC_SPI_TDR, val: 0x%lx\n", val64);
            at32uc_spi_transfer(s);
            break;
        case AT32UC_SPI_CSR0:
            printf("[at32uc_spi_write] AT32UC_SPI_CSR0, val: 0x%lx\n", val64);
            s->spi_csr0 = val64;
            break;
        case AT32UC_SPI_CSR1:
            printf("[at32uc_spi_write] AT32UC_SPI_CSR1, val: 0x%lx\n", val64);
            s->spi_csr1 = val64;
            break;
        case AT32UC_SPI_CSR2:
            printf("[at32uc_spi_write] AT32UC_SPI_CSR2, val: 0x%lx\n", val64);
            s->spi_csr2 = val64;
            break;
        case AT32UC_SPI_CSR3:
            printf("[at32uc_spi_write] AT32UC_SPI_CSR3, val: 0x%lx\n", val64);
            s->spi_csr3 = val64;
            break;
        default:
            printf("[at32uc_spi_write] unknown, addr: 0x0x%lx, val: 0x%lx\n", addr << 2, val64);
            break;
    }

    if(!counter) {
        exit(0);
    }
}

static const MemoryRegionOps spi_ops = {
        .read = at32uc_spi_read,
        .write = at32uc_spi_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_spi_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3SPIState *s = AT32UC3_SPI(dev);
    int i;

    s->spi = ssi_create_bus(dev, "spi");

    sysbus_init_irq(sbd, &s->irq);
    s->cs_lines = g_new0(qemu_irq, s->num_cs);
//    printf("[at32uc3_spi_realize] Using %d chip selects\n", s->num_cs);
    for (i = 0; i < s->num_cs; ++i) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &spi_ops, s, TYPE_AT32UC3_SPI, 0x400); // R_MAX * 4 = size of region
    sysbus_init_mmio(sbd, &s->mmio);

    s->irqline = -1;

    fifo32_create(&s->rx_fifo, FIFO_CAPACITY);
}

static void at32uc3_spi_reset(DeviceState *dev)
{
    at32uc_spi_do_reset(AT32UC3_SPI(dev));
}

static Property at32uc3_spi_properties[] = {
        DEFINE_PROP_UINT8("num-ss-bits", AT32UC3SPIState, num_cs, 4),
        DEFINE_PROP_END_OF_LIST(),
};

static void at32uc3_spi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_spi_realize;
    dc->reset = at32uc3_spi_reset;

    device_class_set_props(dc, at32uc3_spi_properties);
}

static const TypeInfo at32uc3_spi_info = {
        .name           = TYPE_AT32UC3_SPI,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3SPIState),
        .class_init     = at32uc3_spi_class_init,
};

static void at32uc3_spi_register_types(void)
{
    type_register_static(&at32uc3_spi_info);
}

type_init(at32uc3_spi_register_types)