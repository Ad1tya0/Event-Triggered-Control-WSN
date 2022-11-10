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
#define NUM_SENSOR 5 //cant retrieve this from app.c
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


struct forwardCollect_t {//header packet for data collection
    linkaddr_t source;
    uint8_t hops;
} __attribute__((packed));



struct event_msg_t {
    linkaddr_t event_source;
    uint16_t event_seqn;
}__attribute__((packed));

struct connection_t ConnectionBuffer[5];//store 5 best connections here, use some
struct forwardCollect_t collect[NUM_SENSOR];// for data forwarding
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
    BC_FORWARD_REQ,
    BC_FORWARD_ACK
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
        ConnectionBuffer[i].metric = 50;
        ConnectionBuffer[i].rssi = RSSI_THRESHOLD;
        ConnectionBuffer[i].parent = &linkaddr_null;
    }//remove all connections, wrap in function? might reuse
    ctimer_stop(&beacon_timer);
}

void beacon_recv(struct broadcast_header *broadcastHeader, linkaddr_t *sender){ //use header enum instead of callback, should be useful to understand type of message easily

    struct beacon_msg beaconMsg;
    int16_t rssi;
    int i,j =0;
    //struct my_collect_conn *conn; //not gonna use this


//    /* Get the pointer to the overall structure my_collect_conn from its field bc */
//    conn = (struct my_collect_conn *)(((uint8_t *)bc_conn) -
//                                      offsetof(struct my_collect_conn, bc));

    /* Check if the received broadcast packet looks legitimate */
    if (packetbuf_datalen() != sizeof(struct beacon_msg)) {
        printf("my_collect: broadcast of wrong size\n");
        return;
    }
    //memcpy(&beacon, packetbuf_dataptr(), sizeof(struct beacon_msg));
    packetbuf_copyto(&beaconMsg); //this works as well

    //Read the RSSI of the *last* reception.
    rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    printf("recv beacon from %02x:%02x seqn %u metric %u rssi %d\n",
           sender->u8[0], sender->u8[1],
           beacon.seqn, beacon.metric, rssi);

    if (beaconMsg.seqn != 0 && rssi < RSSI_THRESHOLD || beaconMsg.seqn < ConnectionBuffer[0].seqn)
        return;                             // The beacon is either too weak or too old, ignore it
    if (beaconMsg.seqn == ConnectionBuffer[0].seqn) { // The beacon is not new, check the metric
        for (i=0; i<5; i++){
            if(beaconMsg.seqn == 0 && beaconMsg.seqn > ConnectionBuffer[0].seqn)
                break;
            if(beaconMsg.metric + 1 >= ConnectionBuffer[i].metric) // Worse or equal than what we have, ignore it
                continue;   //more hops == bad
            if(rssi <= ConnectionBuffer[i].rssi)    //worse or same rssi means we can try to find better connections
                continue;   //more rssi == awyiss
            break;
        }
        if (i >= 5)//cant store more than 5 connections
            return;
    }

        for(j = 4; j>i; j--){    //put connections in buffer accordingly, updating current index with a better one if found
            linkaddr_copy(&ConnectionBuffer[j].parent, &ConnectionBuffer[j-1].parent);
            ConnectionBuffer[j].seqn = ConnectionBuffer[j-1].seqn;
            ConnectionBuffer[j].metric = ConnectionBuffer[j-1].metric;
            ConnectionBuffer[j].rssi = ConnectionBuffer[j-1].rssi;
        }
        for(j=0; j<5; j++){ //its 5 because i take 5 connections in the connection buffer: ConnectionBuffer[5]
            linkaddr_copy(&ConnectionBuffer[j].parent, &linkaddr_null);
            ConnectionBuffer[j].seqn = 0;
            ConnectionBuffer[j].metric = 0;
            ConnectionBuffer[j].rssi = RSSI_THRESHOLD;
        }//remove all connections in that index

        /* Otherwise, memorize the new parent, the metric, and the seqn, we also store rssi */
        linkaddr_copy(&ConnectionBuffer[i].parent, sender);
        ConnectionBuffer[i].metric = beaconMsg.metric + 1;
        ConnectionBuffer[i].seqn = beaconMsg.seqn;
        ConnectionBuffer[i].rssi = rssi;


        /* TO DO 4://done
* If the metric or the seqn has been updated, retransmit *after a small, random
* delay* (BEACON_FORWARD_DELAY) the beacon message in broadcast, to update the
* node's neighbors about the routing changes
*/
        if(i==0){//first index in buffer is the best possible connection, the next is the second best connection
            /* Schedule beacon propagation */
            ctimer_set(&beacon_timer, BEACON_FORWARD_DELAY, beacon_timer_cb, NULL);
            printf("my_collect: new parent %02x:%02x, my metric %d, rssi %d\n",
               ConnectionBuffer[0]->parent.u8[0], ConnectionBuffer[0].parent.u8[1], ConnectionBuffer[0].metric, ConnectionBuffer[0].rssi);
        }
        else{
            printf("my_collect: second best parent %02x:%02x, my metric %d, rssi %d\n",
                   ConnectionBuffer[i]->parent.u8[0], ConnectionBuffer[i].parent.u8[1], ConnectionBuffer[i].metric, ConnectionBuffer[i].rssi);
        }
    }

struct connection_t *get_BestConnection(void){//call this function to get best possible connection
        return &ConnectionBuffer[0];
    }

    /////////////////////////////////////////Downward Forwarding for actuation//////////////////////////////////////////

    void downForward_Reset(void) {
        for (int i = 0; i < NUM_SENSOR; i++) {
            linkaddr_copy(&collect[i].source, &linkaddr_null);
            collect[i].hops = 50;
        }
    }

    struct forwardCollect_t downForward_Start(linkaddr_t* srcSensor, linkaddr_t* srcHop, uint8_t metric){
    for(int i=0; i< NUM_SENSOR; i++){
        if(linkaddr_cmp(srcSensor, &colect[i].source))
            return &collect[i];
        }
    }





