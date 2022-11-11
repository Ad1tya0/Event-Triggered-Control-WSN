#ifndef __etc_H__
#define __etc_H__
/*---------------------------------------------------------------------------*/
#include <stdbool.h>
#include "contiki.h"
#include "net/rime/rime.h"
#include "net/netstack.h"
#include "core/net/linkaddr.h"
#include "flooding.h"
/*---------------------------------------------------------------------------*/
/* The following are suggested values for the ETC connection.
 * Depending on your implementation, you may not need some of these macros,
 * or you may configure the system with different values.
 * Adapting this configuration to your solution is encouraged.
 * Please mention any modified parameter in the report. */
#define BEACON_INTERVAL             (CLOCK_SECOND * 30)                                     // interval for collection tree reconstruction
#define BEACON_FORWARD_DELAY        (random_rand() % CLOCK_SECOND)                          // random delay to avoid collision in tree beacon propagation
#define EVENT_FORWARD_DELAY         (random_rand() % (CLOCK_SECOND / 10))                   // random delay to avoid collisions in event propagation
#define COLLECT_START_DELAY         (CLOCK_SECOND * 3 + random_rand() % (CLOCK_SECOND * 2)) // delay between event and data collection (with a random component to avoid collisions)
#define SUPPRESSION_TIMEOUT_NEW     (CLOCK_SECOND * 12)                                     // timeout for new event generation (ignore new triggers until it expires)
#define SUPPRESSION_TIMEOUT_PROP    (SUPPRESSION_TIMEOUT_NEW - CLOCK_SECOND / 2)            // timeout for event repropagation (shorter than the previous timeout to ensure forwarders are ready)
//#define SUPPRESSION_TIMEOUT_END   (CLOCK_SECOND / 2)                                      // optional, short timeout after a command is received to disable suppression early
#define MAX_SENSORS                 (10)                                                    // maximum number of sensors in the system
#define RSSI_THRESHOLD              (-95)                                                   // ignore links with RSSI < RSSI_THRESHOLD
/*---------------------------------------------------------------------------*/
typedef enum {
  COMMAND_TYPE_NONE,
  COMMAND_TYPE_RESET,
  COMMAND_TYPE_THRESHOLD
} command_type_t;
/*---------------------------------------------------------------------------*/
typedef enum {
  NODE_ROLE_CONTROLLER,
  NODE_ROLE_SENSOR_ACTUATOR,
  NODE_ROLE_FORWARDER,
  NODE_ROLE_INVALID
} node_role_t;
/*---------------------------------------------------------------------------*/
/* Callback structure */
struct etc_callbacks {

  /* Controller callbacks */
  void (* recv_cb)(const linkaddr_t *event_source, uint16_t event_seqn, const linkaddr_t *source, uint32_t value, uint32_t threshold);
  void (* ev_cb)(const linkaddr_t *event_source, uint16_t event_seqn);

  /* Sensor/actuator callbacks */
  void (* com_cb)(const linkaddr_t *event_source, uint16_t event_seqn, command_type_t command, uint32_t threshold);
};

//sensor previous command structure
static struct {
    linkaddr_t event_source;
    uint16_t event_seqn;
    enum command_type_t cmdtype;
    uint32_t threshold;
} prev_cmd_s;
/*---------------------------------------------------------------------------*/
/* Connection object */
struct etc_conn {

  /* Connections */
  /* ... */

  /* Application callbacks */
  const struct etc_callbacks* callbacks;

  /* Timers */
  /* ... */
  struct ctimer suppression_timer;      // used to stop the generation of new events
  struct ctimer suppression_prop_timer; // used to temporarily stop the propagation of events from other nodes

  /* Role (controller, forwarder, sensor/actuator) */
  node_role_t node_role;

  /* Current event handled by the node;
   * useful to match logs of the control loop till actuation */
  linkaddr_t event_source;
  uint16_t event_seqn;
};
/*---------------------------------------------------------------------------*/
/* Initialize a ETC connection 
 *  - conn -- a pointer to a ETC connection object 
 *  - channels -- starting channel C (ETC may use multiple channels)
 *  - node_role -- role of the node (controller, forwarders, sensor/actuator)
 *  - callbacks -- a pointer to the callback structure
 *  - sensors -- addresses of sensors
 *  - num_sensors -- how many addresses in sensors */
bool etc_open(
  struct etc_conn* conn,
  uint16_t channels, 
  node_role_t node_role,
  const struct etc_callbacks *callbacks,
  linkaddr_t *sensors,
  uint8_t num_sensors);

/* Sensor functions */
void etc_update(uint32_t value, uint32_t threshold);
int etc_trigger(struct etc_conn *conn, uint32_t value, uint32_t threshold);

/* Controller function */
int etc_command(struct etc_conn *conn, const linkaddr_t *dest, command_type_t command, uint32_t threshold);

/* Close connection */
void etc_close(struct etc_conn *conn);

/*---------------------------------------------------------------------------*/
#endif /* __etc_H__ */
