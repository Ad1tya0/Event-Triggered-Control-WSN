//
// Created by Ad1tya on 08-11-2022.
//

#include "flooding.h"
#include "etc.h"
#include "net/mac/mac.h"
#include "net/netstack.h"
#include "net/rime/rime.h"
#include "net/packetbuf.h"


struct beacon_msg {
    uint16_t seqn;
    uint16_t metric;
} __attribute__((packed));

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
        .sent = NULL};

enum broadcast_msg_type {
    BC_TYPE_BEACON,
    BC_TYPE_EVENT,
    BC_COLLECT_REQ,
    BC_COLLECT_ACK
};

struct broadcast_header {
    enum broadcast_msg_type_t type;
} __attribute__((packed));














//unicast
struct unicast_callbacks uc_cb = {
        .recv = uc_recv,
        .sent = NULL};

enum unicast_msg_type {
    UC_TYPE_COLLECT,
    UC_TYPE_COMMAND
}__attribute__((packed));

struct unicast_header {
    enum unicast_msg_type type;
    uint8_t metric;
} __attribute__((packed));



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



