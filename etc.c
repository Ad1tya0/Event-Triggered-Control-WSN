#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "etc.h"
#include <net/mac/mac.h>
#include <net/packetbuf.h>
#include "flooding.h"
/*---------------------------------------------------------------------------*/
/* A simple debug system to enable/disable some printfs */
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*---------------------------------------------------------------------------*/
/* Topology information (parents, metrics...) */
/* ... */
/*---------------------------------------------------------------------------*/
/* Forwarders (routes to sensors/actuators) */

/* ... */
/*---------------------------------------------------------------------------*/
/* Declarations for the callbacks of dedicated connection objects */
/* ... */
/*---------------------------------------------------------------------------*/
/* Rime Callback structures */
/* ... */
/*---------------------------------------------------------------------------*/
/*                           Application Interface                           */
/*---------------------------------------------------------------------------*/

struct ctimer holdtaskTimer;

/* Create connection(s) and start the protocol */
bool etc_open(struct etc_conn* conn, uint16_t channels,
         node_role_t node_role, const struct etc_callbacks *callbacks,
         linkaddr_t *sensors, uint8_t num_sensors)
{

    connectivity_BEGIN(channels);
  /* Initialize the connector structure */

  /* Open the underlying Rime primitives */


  /* Initialize sensors forwarding structure */

  /* Tree construction (periodic) */
}
/*---------------------------------------------------------------------------*/
/* Turn off the protocol */
void etc_close(struct etc_conn *conn)
{
    ctimer_stop(&holdtaskTimer;)
    connectivity_TERMINATE();
  /* Turn off connections to ignore any incoming packet
   * and stop transmitting */
}
/*---------------------------------------------------------------------------*/
/* Used by the app to share the most recent sensed value;
 * ONLY USED BY SENSORS */
uint32_t sensorVal= value;
uint32_t sensorThreshold = threshold;
void etc_update(uint32_t value, uint32_t threshold)
{
    sensorVal= value;
    sensorThreshold = threshold;
  /* Update local value and threshold, to be sent in case of event */
}
/*---------------------------------------------------------------------------*/
/* Start event dissemination (unless events are being suppressed to avoid
 * contention).
 * Returns 0 if new events are currently being suppressed.
 * ONLY USED BY SENSORS */

int etc_trigger(struct etc_conn *conn, uint32_t value, uint32_t threshold)
{
  /* Prepare event message */
    struct unicast_header triguc;
    struct broadcast_header trigbc;
    triguc.type = UC_TYPE_COLLECT;
    trigbc.bcType = BC_TYPE_EVENT;
  /* Suppress other events for a given time window */
    ctimer_set(holdtaskTimer, (CLOCK_SECOND * 10), NULL, NULL);
  /* Send event */
    ucast_send(triguc, get_BestConnection()->parent);
    bcast_send_type(trigbc);
}
/*---------------------------------------------------------------------------*/
/* Called by the controller to send commands to a given destination.
 * ONLY USED BY CONTROLLER */
int etc_command(struct etc_conn *conn, const linkaddr_t *dest,
            command_type_t command, uint32_t threshold)
{
  /* Prepare and send command */
  struct unicast_header commandHeader;
  struct command_msg_t commandMessage;

  commandHeader.type = UC_TYPE_COMMAND;
  commandHeader.metric =0;
  commandMessage.event_seqn=0;
  linkaddr_copy(&commandMessage.event_source, &linkaddr_node_addr);

    ucast_send(commandHeader, collect[0].source);
}
/*---------------------------------------------------------------------------*/
/*                              Beacon Handling                              */
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/
/*                               Event Handling                              */
/*---------------------------------------------------------------------------*/
/* Event message structure, combining event source (address of the sensor
 * generating the event) and a sequence number. */
//struct event_msg_t {
//  linkaddr_t event_source;
//  uint16_t event_seqn;
//}__attribute__((packed));//porting to flood
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/
/*                               Data Handling                               */
/*---------------------------------------------------------------------------*/
/* Structure for data collection messages.
 * Note: does not require separation of header and payload. */
//struct collect_msg_t {
//  linkaddr_t event_source;
//  uint16_t event_seqn;
//  /* ... */
//}__attribute__((packed)); //ported to flooding
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/
/*                               Command Handling                            */
/*---------------------------------------------------------------------------*/
/* Structure for command messages.
 * Note: does not require separation of header and payload. */
//struct command_msg_t {
//  linkaddr_t event_source;
//  uint16_t event_seqn;
//  /* ... */
//}__attribute__((packed)); //ported to flooding
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/