/*
 * QEMU AVR32 UART
 *
 * Copyright (c) 2023, Florian Göhler
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
#include "qemu/module.h"
#include "hw/avr32/at32uc3_uart.h"
#include "migration/vmstate.h"
#include "qemu/main-loop.h"

#define CR 0x0 //Control Register
#define MR 0x4 //Mode Register
#define IER 0x8 //Interrupt Enable Register
#define IDR 0xC //Interrupt Disable Register
#define IMR 0x10 //Interrupt Mask Register
#define CSR 0x14 //Interrupt Mask Register
#define RHR 0x18 //Receive Holding Register
#define THR 0x1C //Transmit Holding Register
#define RTOR 0x24 //Receiver Time-out Register
#define TTGR 0x28 //Transmitter Timeguard Register

#define CSR_RXRDY (1 << 0)
#define CSR_TXRDY (1 << 1)

static FILE *fileOut;
int uart_client_sock = -1;
int uart_server_port = 10101;
int uart_s_socket;
struct sockaddr_in uart_client_addr, uart_server_addr;
socklen_t uart_client_size;
AT32UC3UARTState* uart_state;
static bool uart_client_connected = false;

static uint64_t at32uc_uart_read(void *opaque, hwaddr addr, unsigned int size)
{
    AT32UC3UARTState* s = AT32UC3_UART(opaque);

    int offset = (int) addr;
    int returnValue = 0;
    switch (offset) {
        case MR:
            returnValue = s->mr;
            printf("[opssat_uart_read] MR: 0x%x\n", returnValue);
            break;
        case IMR:
            returnValue = s->imr;
            printf("[opssat_uart_read] IMR: 0x%x\n", returnValue);
            break;
        case CSR:{
            returnValue = s->csr;
//            printf("[opssat_uart_read] CSR: 0x%x\n", returnValue);
            break;
        }
        case RHR:{
            returnValue = s->rhr;
            printf("[opssat_uart_read] RHR: 0x%x\n", returnValue);
            s->csr &= ~CSR_RXRDY;
            break;
        }
        case RTOR:{
            returnValue = s->rtor;
            printf("[opssat_uart_read] RTOR: 0x%x\n", returnValue);
            break;
        }
        case TTGR:{
            returnValue = s->ttgr;
            printf("[opssat_uart_read] TTGR: 0x%x\n", returnValue);
            break;
        }
        default:
            printf("[opssat_uart_read] Not implemented: 0x%lx\n", addr);
            break;
    }
    return returnValue;
}

static void at32uc_uart_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    AT32UC3UARTState* s = AT32UC3_UART(opaque);
    int offset = (int) addr;
    switch (offset) {
        case CR:{
            s->cr = (u_int32_t) val64;
            printf("[opssat_uart_write] CR: 0x%lx\n", val64);
            break;
        }
        case MR:{
            s->mr = (u_int32_t) val64;
            printf("[opssat_uart_write] Mode: 0x%lx\n", val64);
            break;
        }
        case IDR:{
            printf("[opssat_uart_write] IDR: 0x%lx. New IMR: ", val64);
            s->idr = (u_int32_t) val64;
            s->imr &= ~s->idr;
            printf("0x%x\n", s->imr);
            break;
        }
        case THR:
            s->buf[s->buf_idx] = (char)val64;
            s->buf_idx++;
            if(uart_client_connected){
                send(uart_client_sock, &s->buf[s->buf_idx-1], 1, 0);
            }
            fprintf(fileOut, "%c", (char)val64);
            fflush(fileOut);
            break;
        case RTOR:{
            s->rtor = (u_int32_t) val64;
            break;
        }
        case TTGR:{
            s->ttgr = (u_int32_t) val64;
            break;
        }
        default:
            printf("[opssat_uart_write] Not implemented: 0x%lx\n", addr);
            break;
    }
}

static int init_uart_server(void){
    int uart_s_socket = socket(AF_INET, SOCK_STREAM, 0);

    uart_server_addr.sin_family = AF_INET;
    uart_server_addr.sin_port = htons(uart_server_port);
    uart_server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    uart_client_size = sizeof(uart_client_addr);

    int bind_res;
    while(true){
        bind_res = bind(uart_s_socket, (struct sockaddr*)&uart_server_addr, sizeof(uart_server_addr));
        if(bind_res < 0){
            uart_server_port++;
            uart_server_addr.sin_port = htons(uart_server_port);
        }
        else {
            printf("[opssat_uart_thread] Server bound to port %i @ 0.0.0.0\n", uart_server_port);
            break;
        }
    }
    return uart_s_socket;
}


static int handle_new_client(AT32UC3UARTState *s){
    if (uart_client_sock < 0) {
        printf("[opssat_uart_thread] Waiting for connection...\n");
        uart_client_sock = accept(uart_s_socket, (struct sockaddr*)&uart_client_addr, &uart_client_size);
        if (uart_client_sock < 0) {
            printf("[opssat_uart_thread] Client acceptance error\n");
            return 0;
        }
        printf("=================================================================\n");
        printf("\n");
        printf("\n");
        printf("\n");
        printf("[opssat_uart_thread] New client connected from %s\n", inet_ntoa(uart_client_addr.sin_addr));
        for(int i =0; i < s->buf_idx; i++){
            send(uart_client_sock, &s->buf[i], 1, 0);
        }
    }
    return 1;
}

static void* uart_thread(void *opaque){
    uart_s_socket = init_uart_server();
    AT32UC3UARTState *s = AT32UC3_UART(opaque);

    while(1) {

        int listen_result = listen(uart_s_socket, 1);
        if(listen_result < 0){
            printf("[opssat_uart_thread] TCP listening error. Restarting loop.\n");
            continue;
        }

        if(!handle_new_client(s)){
            continue;
        }
        uart_client_connected = true;
        printf("[opssat_uart_thread] Sending buffered messaged: %i bytes\n", s->buf_idx);

        for(int i= 0; i < s->buf_idx; i++){
            send(uart_client_sock, &s->buf[i], 1, 0);
        }

        char incoming_message[1];
        int recv_result;
        while(1){
            recv_result = recv(uart_client_sock, incoming_message, sizeof(incoming_message), 0);
            if(recv_result > 0){
                printf("[opssat_uart_thread] INPUT: %c (0x%x)\n", incoming_message[0], incoming_message[0]);
                s->rhr = incoming_message[0];
                s->csr |= CSR_RXRDY;
                qemu_mutex_lock_iothread();
                qemu_set_irq(s->irq, 2);
                qemu_mutex_unlock_iothread();
            }
            else if(recv_result <= 0){
                printf("[opssat_uart_thread] CLOSED!\n");
                break;
            }
        }
    }
    return NULL;
}

static const MemoryRegionOps uart_ops = {
        .read = at32uc_uart_read,
        .write = at32uc_uart_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
                .min_access_size = 4,
                .max_access_size = 4
        }
};

static void at32uc3_uart_realize(DeviceState *dev, Error **errp)
{
    fileOut = fopen("uart_output.txt", "w");

    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AT32UC3UARTState *s = AT32UC3_UART(dev);
    int i;

    sysbus_init_irq(sbd, &s->irq);
    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    for (i = 0; i < s->num_cs; ++i) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &uart_ops, s, TYPE_AT32UC3_UART, 0x100); // R_MAX * 4 = size of region
    sysbus_init_mmio(sbd, &s->mmio);

    s->irqline = -1;
    s->buf_idx = 0;
    memset(s->buf, '\0', sizeof(s->buf));
    qemu_thread_create(&s->uart_thread, "nanomind.uart", uart_thread, s, QEMU_THREAD_JOINABLE);
}

static void at32uc3_uart_reset(DeviceState *dev)
{
    AT32UC3UARTState *s = AT32UC3_UART(dev);
    s->csr = 0 | CSR_TXRDY;
    //TODO: Implement all registers
}

static void at32uc3_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = at32uc3_uart_realize;
    dc->reset = at32uc3_uart_reset;
}

static const TypeInfo at32uc3_uart_info = {
        .name           = TYPE_AT32UC3_UART,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AT32UC3UARTState),
        .class_init     = at32uc3_uart_class_init,
};

static void at32uc3_uart_register_types(void)
{
    type_register_static(&at32uc3_uart_info);
}

type_init(at32uc3_uart_register_types)