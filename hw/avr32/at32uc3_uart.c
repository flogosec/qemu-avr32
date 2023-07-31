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


#define SR 0x4 //Status Register
#define IMR 0x14 //Interrupt Mask Register
#define RHR 0x18 //Receive Holding Register
#define THR 0x1C //Transmit Holding Register

static int baudrate = 0;
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
        case SR:
            returnValue = baudrate;
            break;
        case IMR:
            returnValue = 0xFF;
            break;
        case RHR:{
            printf("[opssat_uart_thread] READ: %c!\n", s->rhr);
            returnValue = s->rhr;
        }
        default:
            break;
    }
    return returnValue;
}

static void at32uc_uart_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    AT32UC3UARTState* s = AT32UC3_UART(opaque);
    int offset = (int) addr;
    switch (offset) {
        case SR:
            printf("[at32uc3_uart_write] SR is read-only!\n");
            break;
        case THR:
            if(val64 == 0x30){
                return;
            }
            s->buf[s->buf_idx] = (char)val64;
            s->buf_idx++;
            if(uart_client_connected){
                send(uart_client_sock, &s->buf[s->buf_idx-1], 1, 0);
            }
            fprintf(fileOut, "%c", (char)val64);
            fflush(fileOut);
            break;
        default:
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

        char incoming_message[1];
        int recv_result;
        while(1){
            recv_result = recv(uart_client_sock, incoming_message, sizeof(incoming_message), 0);
            if(recv_result > 0){
                printf("[opssat_uart_thread] INPUT: %c (0x%x)\n", incoming_message[0], incoming_message[0]);
                s->rhr = incoming_message[0];
                qemu_mutex_lock_iothread();
                qemu_set_irq(s->irq, 2);
                qemu_mutex_unlock_iothread();
            }
            else if(recv_result <= 0){
                printf("[opssat_uart_thread] CLOSED!\n");
                break;
            }
        }
        /*

        int recv_result = readMessage();
        if(recv_result <= 0){
            continue;
        }
        qemu_mutex_lock_iothread();

        handle_received_message();*/
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