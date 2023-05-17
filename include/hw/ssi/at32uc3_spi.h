#ifndef QEMU_AVR32_AT32UC3_SPI_H
#define QEMU_AVR32_AT32UC3_SPI_H

#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qom/object.h"
#include "qemu/fifo32.h"
#include "hw/irq.h"

#define TYPE_AT32UC3_SPI "at32uc3.spi"

struct AT32UC3SPIState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    qemu_irq irq;
    int irqline;

    uint8_t num_cs;
    qemu_irq *cs_lines;

    SSIBus *spi;

    Fifo32 rx_fifo;

    uint32_t spi_cr;
    uint32_t spi_mr;
    uint16_t spi_tdr;
    uint32_t spi_sr;
    uint16_t spi_rdr;
    u_int64_t spi_csr0;
    u_int64_t spi_csr1;
    u_int64_t spi_csr2;
    u_int64_t spi_csr3;

    uint16_t shift_reg;
};

typedef struct AT32UC3SPIState AT32UC3SPIState;

DECLARE_INSTANCE_CHECKER(AT32UC3SPIState, AT32UC3_SPI, TYPE_AT32UC3_SPI)

#endif //QEMU_AVR32_AT32UC3_SPI_H
