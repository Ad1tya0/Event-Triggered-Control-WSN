//
// Created by Ad1tya on 08-11-2022.
//

#include "flooding.h"
#include "etc.h"
#include "net/mac/mac.h"
#include "net/netstack.h"
#include "net/rime/rime.h"
#include "net/packetbuf.h"

//#define BEACON_INTERVAL (CLOCK_SECOND * 60) /* Try to change this period to analyse \
//                                             * how it affects the radio-on time     \
//                                             * (energy consumption) of ContikiMac */

linkaddr_t etc_controllerx = {{0xF7, 0x9C}};//stupid method, should port node allocator from app.c, but if it works it works ig
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

struct event_msg_t {
    linkaddr_t event_source;
    uint16_t event_seqn;
}__attribute__((packed));

struct connection_t ConnectionBuffer[5];//store 5 best connections here, use some

/* Callback function declarations */
void bc_recv(struct broadcast_conn *conn, const linkaddr_t *sender);
void uc_recv(struct unicast_conn *c, const linkaddr_t *from);
void beacon_timer_cb(void *ptr);
/*---------------------------------------------------------------------------*/
/* Rime Callback structures */
//broadcast
struct broadcast_conn bc_conn;


struct broadcast_callbacks bc_cb = {
        .recv = bc_recv,
        .sent = bc_sent};

enum broadcast_msg_type {
    BC_TYPE_BEACON,
    BC_TYPE_EVENT,
    BC_COLLECT_REQ,
    BC_COLLECT_ACK
}__attribute__((packed));

struct broadcast_header {
    enum broadcast_msg_type_t type;
}__attribute__((packed));


//unicast
struct unicast_conn uc_conn;

struct unicast_callbacks uc_cb = {
        .recv = uc_recv,
        .sent = uc_sent};

enum unicast_msg_type {
    UC_TYPE_COLLECT,
    UC_TYPE_COMMAND
}__attribute__((packed));

struct unicast_header {
    enum unicast_msg_type type;
    uint8_t metric;
}__attribute__((packed));



/*---------------------------------------------------------------------------*/
                        /* Beacon routines */

void send_beacon(const struct beacon_msg *beaconMsg){
    packetbuf_clear();
    packetbuf_copyfrom(beaconMsg, sizeof(struct beacon_msg));

    struct broadcast_header bcastHeader = {.type = BC_TYPE_BEACON};
    if (!packetbuf_hdralloc(sizeof (bcastHeader))){
        printf(("FAILED to allocate beacon header: seqn %d metric %d\n", beaconMsg->seqn, beaconMsg->metric);
        return;
    }
    memcpy(packetbuf_hdrptr(), &bcastHeader, sizeof (bcastHeader));

    bool ret = broadcast_send(&bc_conn);
    if(!ret)
        printf(("FAILED to send beacon: seqn %d metric %d\n", beaconMsg->seqn, beaconMsg->metric);
    else
        printf("sending beacon: seqn %d metric %d\n", beaconMsg->seqn, beaconMsg->metric);
}


void beacon_timer_cb(void *ptr){
//    struct my_collect_conn *conn = (struct my_collect_conn *)ptr; //what??
    struct beacon_msg beaconMsg = {.seqn = ConnectionBuffer[0].seqn,
                                   .metric = ConnectionBuffer[0].metric};
    send_beacon(&beaconMsg);//start flooding
    if (linkaddr_cmp(&etc_controllerx, &linkaddr_node_addr)){//verify if controller, rebuild the three from scratch after beacon interval
        ConnectionBuffer[0].seqn++;//update beacon number
        ctimer_set(&beacon_timer, BEACON_INTERVAL, beacon_timer_cb, NULL);
    }
}


void beacon_Start(void){
    //start by removing all connections
    for(int i=0; i<5; i++){
        ConnectionBuffer[i].seqn = 0;
        ConnectionBuffer[i].metric = 0;
        ConnectionBuffer[i].rssi = RSSI_THRESHOLD;
        ConnectionBuffer[i].parent = &linkaddr_null;
    }//remove all connections, wrap in function? might reuse
    if (linkaddr_cmp(&etc_controllerx, &linkaddr_node_addr)) {//verify if controller
        ctimer_set(&beacon_timer, CLOCK_SECOND, beacon_timer_cb, NULL);//Schedule the first beacon message flood
        ConnectionBuffer[0].metric = 0;/* The sink hop count is (by definition) *always* equal to 0.
                       * Remember to update this field *before sending the first*
                       * beacon message in broadcast! */
    }
}

void beacon_Stop(void){
    for(int i=0; i<5; i++){ //its 5 because i take 5 connections in the connection buffer: ConnectionBuffer[5]
        ConnectionBuffer[i].seqn = 0;
        ConnectionBuffer[i].metric = 0;
        ConnectionBuffer[i].rssi = RSSI_THRESHOLD;
        ConnectionBuffer[i].parent = &linkaddr_null;
    }//remove all connections, wrap in function? might reuse
    ctimer_stop(&beacon_timer);
}

void beacon_recv(struct broadcast_header *broadcastHeader, linkaddr_t *sender){ //use header enum instead of callback, should be useful to understand type of message easily

    struct beacon_msg beaconMsg;
//do beacon tree logic with hops and rssi
}

