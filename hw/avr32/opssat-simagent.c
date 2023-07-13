/*
 * OPSSAT Simulation Agent
 *
 * Copyright (c) 2023 Florian Göhler, Johannes Willbold
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
#include "qom/object.h"
#include "hw/qdev-properties.h"
#include "qemu/main-loop.h"

#include "opssat-simagent.h"

#define CSP_BIG_ENDIAN
#define CSP_PADDING_BYTES 8

/** Size of bit-fields in CSP header */
#define CSP_ID_PRIO_SIZE		2
#define CSP_ID_HOST_SIZE		5
#define CSP_ID_PORT_SIZE		6
#define CSP_ID_FLAGS_SIZE		8

typedef union {
    uint32_t ext;
    struct __attribute__((__packed__)) {
#if defined(CSP_BIG_ENDIAN) && !defined(CSP_LITTLE_ENDIAN)
        unsigned int pri : CSP_ID_PRIO_SIZE;
		unsigned int src : CSP_ID_HOST_SIZE;
		unsigned int dst : CSP_ID_HOST_SIZE;
		unsigned int dport : CSP_ID_PORT_SIZE;
		unsigned int sport : CSP_ID_PORT_SIZE;
		unsigned int flags : CSP_ID_FLAGS_SIZE;
#elif defined(CSP_LITTLE_ENDIAN) && !defined(CSP_BIG_ENDIAN)
        unsigned int flags : CSP_ID_FLAGS_SIZE;
		unsigned int sport : CSP_ID_PORT_SIZE;
		unsigned int dport : CSP_ID_PORT_SIZE;
		unsigned int dst : CSP_ID_HOST_SIZE;
		unsigned int src : CSP_ID_HOST_SIZE;
		unsigned int pri : CSP_ID_PRIO_SIZE;
#else
#error "Must define one of CSP_BIG_ENDIAN or CSP_LITTLE_ENDIAN in csp_platform.h"
#endif
    };
} csp_id_t;

int client_sock = -1;
int server_port = 10001;
int s_socket;
char incoming_message[1000];
struct sockaddr_in client_addr, server_addr;
socklen_t client_size;

typedef struct __attribute__((__packed__)) {
    csp_id_t id;
    uint8_t data[140];
} net_csp_packet_t;

static int init_sim_server(void){
    int s_socket = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    client_size = sizeof(client_addr);

    if(bind(s_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("[opssat_sim_thread] Failed to create socket, aborting!\n");
        exit(-1);
    } else {
        printf("[opssat_sim_thread] Server bound to port 10001 @ 0.0.0.0\n");
    }
    return s_socket;
}

static int handle_new_client(void){
    if (client_sock < 0) {
        printf("[opssat_sim_thread] Waiting for connection...\n");
        client_sock = accept(s_socket, (struct sockaddr*)&client_addr, &client_size);
        if (client_sock < 0) {
            printf("[opssat_sim_thread] Client acceptance error\n");
            return 0;
        }
        printf("=================================================================\n");
        printf("\n");
        printf("\n");
        printf("\n");
        printf("[opssat_sim_thread] New client connected from %s\n", inet_ntoa(client_addr.sin_addr));
    }
    return 1;
}

static int readMessage(void){
    int recv_result = recv(client_sock, incoming_message, sizeof(incoming_message), 0);
    if(recv_result <= 0){
        printf("[opssat_sim_thread] Connection closed. Restarting\n");
        client_sock = -1;
        return 0;
    }
    return recv_result;
}


static void* opssat_sim_thread(void *opaque)
{
    s_socket = init_sim_server();
    OpsSatSimAgentState* s = opaque;

    while(1) {
        memset(incoming_message, '\0', sizeof(incoming_message));

        int listen_result = listen(s_socket, 1);
        if(listen_result < 0){
            printf("[opssat_sim_thread] TCP listening error. Restarting loop.\n");
            continue;
        }

        if(!handle_new_client()){
            continue;
        }

        int recv_result = readMessage();
        if(recv_result <= 0){
            continue;
        }

        printf("[opssat_sim_thread] Received %d bytes of data\n", recv_result);
        qemu_mutex_lock_iothread();

        net_csp_packet_t* packet = (net_csp_packet_t*) g_malloc0(sizeof(net_csp_packet_t));
        packet->id.ext = ((uint8_t)incoming_message[0] << 24) | ((uint8_t)incoming_message[1] << 16) | (((uint8_t)incoming_message[2]) << 8) | (uint8_t)incoming_message[3];
        printf("[opssat_sim_thread] id.ext=0x%x\n", packet->id.ext);

        memcpy(packet->data, &incoming_message[4], sizeof(packet->data));
        packet->id.ext = htonl(packet->id.ext);

        printf("[opssat_sim_thread] Emitting Packet=\n\t");
        int counter = 0;
        for (int i = 0; i < sizeof(net_csp_packet_t); ++i) {
            printf("%02x ", ((uint8_t*) packet)[i]);
            counter++;
            if(counter == 16){
                counter = 0;
                printf("\n\t");
            }
        }
        printf("\n");

        nanocom_ax100_send_packet(s->nanocom, (char*) packet, sizeof(net_csp_packet_t));

        qemu_mutex_unlock_iothread();
    }

    return NULL;
}

void opssat_simagent_nancom_recv_pkt(OpsSatSimAgentState* s, uint8_t* buf, uint32_t size)
{
    printf("[opssat_simagent_nancom_recv_pkt] Recorded Packet=\n\t");
    for (int i = 0; i < size; ++i) {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    if(client_sock >= 0){
        printf("[opssat_simagent_nancom_recv_pkt] Trying to transmit recorded packet via TCP...\n");
        int res = send(client_sock, buf, size, 0);

        if(res > 0){
            printf("[opssat_simagent_nancom_recv_pkt] Transmission successful\n");
        }
        else{
            printf("[opssat_simagent_nancom_recv_pkt] Transmission FAILED! (%d)\n", res);
        }
    }
}

static void opssat_sim_realize(DeviceState *dev, Error **errp)
{
//    OpsSatSimAgentState* s = OPSSAT_SIMAGENT(dev);
//    printf("[opssat_sim_realize] REALIZE\n");
}

static void opssat_sim_unrealize(DeviceState *dev)
{
//    OpsSatSimAgentState* s = OPSSAT_SIMAGENT(dev);
//    printf("[opssat_sim_realize] UNREALIZE\n");
}

static void opssat_sim_init(Object* obj)
{
    OpsSatSimAgentState* s = OPSSAT_SIMAGENT(obj);

    s->nanocom = NULL;

    qemu_thread_create(&s->sim_thread, "opssat.sigmagent", opssat_sim_thread, s, QEMU_THREAD_JOINABLE);
}

static void opssat_sim_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

//    dc->reset = opssat_sim_reset;
    dc->realize = opssat_sim_realize;
    dc->unrealize = opssat_sim_unrealize;
}


static const TypeInfo opssat_simagent_types[] = {
        {
                .name          = TYPE_OPSSAT_SIMAGENT,
                .parent        = TYPE_DEVICE,
                .instance_size = sizeof(OpsSatSimAgentState),
                .instance_init = opssat_sim_init,
                .class_init    = opssat_sim_class_init,
        }
};

DEFINE_TYPES(opssat_simagent_types)