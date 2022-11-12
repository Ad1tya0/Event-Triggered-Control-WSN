//
// Created by Ad1tya on 08-11-2022.
//

#ifndef EVENT_TRIGGERED_CONTROL_WSN_FLOODING_H
#define EVENT_TRIGGERED_CONTROL_WSN_FLOODING_H



#include "net/linkaddr.h"
#include "sys/types.h"
#include "stdbool.h"
#include "etc.h"
#include "net/mac/mac.h"
#include "net/netstack.h"
#include "net/rime/rime.h"
#include "net/packetbuf.h"


#define NUM_SENSOR 5
//linkaddr_t etc_controllerx = {{0xF7, 0x9C}};
struct beacon_msg {
    uint16_t seqn;
    uint16_t metric;
} __attribute__((packed));


static struct ctimer beacon_timer;

struct connection_t {//use for making a buffer for connections?
    uint16_t seqn;
    uint16_t metric;
    uint16_t rssi;
    linkaddr_t parent;
};


struct forwardCollect_t {//header packet for data collection
    linkaddr_t source;
    uint8_t metric;
} __attribute__((packed));



struct event_msg_t {
    linkaddr_t event_source;
    uint16_t event_seqn;
}__attribute__((packed));

struct command_msg_t {
    linkaddr_t event_source;
    uint16_t event_seqn;
    /* ... */
}__attribute__((packed));

struct collect_msg_t {
    linkaddr_t event_source;
    uint16_t event_seqn;
    /* ... */
}__attribute__((packed));

struct connection_t ConnectionBuffer[5];//store 5 best connections here, use some
struct forwardCollect_t collect[NUM_SENSOR];// for data forwarding
/* Callback function declarations */
void bc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender);
void uc_recv(struct unicast_conn *uc_conn, const linkaddr_t *from);
void bc_sent(struct broadcast_conn *bc_conn, int status, int num_tx);
void uc_sent(struct broadcast_conn *uc_conn, int status, int num_tx);
void beacon_timer_cb(void *ptr);
/*---------------------------------------------------------------------------*/

//broadcast
//struct broadcast_conn bc_conn;


// struct broadcast_callbacks bc_cb = {
//         .recv = bc_recv,
//         .sent = bc_sent};

enum broadcast_msg_type {
    BC_TYPE_BEACON,
    BC_TYPE_EVENT,
    BC_FORWARD_REQ,
    BC_FORWARD_ACK
}__attribute__((packed));

struct broadcast_header {
    enum broadcast_msg_type bcType;
}__attribute__((packed));


//unicast
//struct unicast_conn uc_conn;

// struct unicast_callbacks uc_cb = {
//         .recv = uc_recv,
//         .sent = uc_sent};

enum unicast_msg_type {
    UC_TYPE_COLLECT,
    UC_TYPE_COMMAND
}__attribute__((packed));

struct unicast_header {
    enum unicast_msg_type type;
    uint8_t metric;
}__attribute__((packed));



/* Rime Callback structures */
struct connection_callbacks {
    // broadcast callback
    struct bc_t {
        void (*recv)(struct broadcast_header *header, linkaddr_t *sender);
        void (*sent)(int status, int num_tx);
    } bc;

    // unicast callback
    struct uc_t {
        void (*recv)(struct unicast_header *header, linkaddr_t *sender);
        void (*sent)(int status);
    } uc;
}connCallback;


void beacon_Start(void);
void beacon_Stop(void);
void send_beacon(const struct beacon_msg *beaconMsg);
void beacon_recv(struct broadcast_header *broadcastHeader, linkaddr_t *sender);
struct connection_t *get_BestConnection(void);
void downForward_Reset(void);
struct forwardCollect_t* downForward_Start(const linkaddr_t* srcSensor, linkaddr_t* srcHop, uint8_t metric);
void bcast_send_type(enum broadcast_msg_type bcType);
void ucast_send(struct unicast_header *ucHeader, linkaddr_t *recvr);
void connectivity_BEGIN(uint16_t channel);
void connectivity_TERMINATE();





#endif //EVENT_TRIGGERED_CONTROL_WSN_FLOODING_H