/*---------------------------------------------------------------------------
 *
 * Event-Triggered Control
 * LPIoT Project 2021/2022
 *
 * This is the template for the project.
 * Many comments have been added to help with the implementation.
 * However, note that hints in comments are not exhaustive;
 * they are just meant to show what is the main purpose of each function.
 *
 *---------------------------------------------------------------------------*/
#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "dev/button-sensor.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "etc.h"
#include "simple-energest.h"
/*---------------------------------------------------------------------------*/
#define ETC_FIRST_CHANNEL           (0xAA)

/* You can increase the controller wait time to improve collection PDR,
 * if needed in your solution. In that case, specify the new value used in
 * your final report and describe the execution conditions in which the change
 * is necessary. */
#define CONTROLLER_COLLECT_WAIT     (CLOCK_SECOND * 10)

/* You can change the values hereafter to increase / decrease the rate
 * of event detection of the system. In the final report, please REMEMBER to
 * mention how you defined such variables for each experiment! */
#define SENSOR_UPDATE_INTERVAL      (CLOCK_SECOND * 7)
#define SENSOR_UPDATE_INCREMENT     (random_rand() % 300)
#define SENSOR_STARTING_VALUE_STEP  (1000)
#define CONTROLLER_MAX_DIFF         (10000)
#define CONTROLLER_MAX_THRESHOLD    (50000)
#define CONTROLLER_CRITICAL_DIFF    (15000)
/*---------------------------------------------------------------------------*/
#ifndef CONTIKI_TARGET_SKY
linkaddr_t etc_controller = {{0xF7, 0x9C}}; /* Firefly node 1 will be our etc_controller */
#define NUM_SENSORS 5
linkaddr_t etc_sensors[NUM_SENSORS] = {
  {{0xF3, 0x84}}, /* Firefly node 3 will be one of our sensor-actuator nodes */
  {{0xF2, 0x33}}, /* Firefly node 12 will be one of our sensor-actuator nodes */
  {{0xf3, 0x8b}}, /* Firefly node 18 will be one of our sensor-actuator nodes */
  {{0xF3, 0x88}}, /* Firefly node 22 will be one of our sensor-actuator nodes */
  {{0xF7, 0xE1}}  /* Firefly node 30 will be one of our sensor-actuator nodes */
};
#else
linkaddr_t etc_controller = {{0x01, 0x00}}; /* Sky node 1 will be our etc_controller */
#define NUM_SENSORS 5
linkaddr_t etc_sensors[] = {
  {{0x02, 0x00}},
  {{0x03, 0x00}},
  {{0x04, 0x00}},
  {{0x05, 0x00}},
  {{0x06, 0x00}}
};
#endif
/*---------------------------------------------------------------------------*/
PROCESS(app_process, "App process");
AUTOSTART_PROCESSES(&app_process);
/*---------------------------------------------------------------------------*/
/* ETC connection */
/*---------------------------------------------------------------------------*/
static struct etc_conn etc;
static void recv_cb(const linkaddr_t *event_source, uint16_t event_seqn, const linkaddr_t *source, uint32_t value, uint32_t threshold);
static void ev_cb(const linkaddr_t *event_source, uint16_t event_seqn);
static void com_cb(const linkaddr_t *event_source, uint16_t event_seqn, command_type_t command, uint32_t threshold);

struct etc_callbacks cb = {.recv_cb = NULL, .ev_cb = NULL, .com_cb = NULL};
const struct event_msg_t *currentEvent(void) {
    return &event;
}
/*---------------------------------------------------------------------------*/
/* Sensor */
/*---------------------------------------------------------------------------*/
static bool is_sensor;
static uint32_t sensor_value;
static uint32_t sensor_threshold;
static struct ctimer sensor_timer;
static struct ctimer collectionTimer; //receiver timer
static void sensor_timer_cb(void* ptr);


/*---------------------------------------------------------------------------*/
/* Controller */
/*---------------------------------------------------------------------------*/
/* Array for sensor readings */
typedef struct {
  linkaddr_t addr;
  uint16_t seqn;
  bool reading_available;
  uint32_t value;
  uint32_t threshold;
  command_type_t command;
} sensor_reading_t;
static sensor_reading_t sensor_readings[NUM_SENSORS];
static uint8_t num_sensor_readings;

/* Actuation functions */
static void actuation_logic();
static void actuation_commands();

/* timer callbacks */
static void collectionTimer_cb(void *ptr) { //called from recv_cb to do actuator after successfully reading sensor value
    actuation_logic();
    actuation_commands();
}
/*---------------------------------------------------------------------------*/
/*                           Node Allocator                                    */
//would need for beacon tree building, port elsewhere?
static enum node_role_t nodeType_returnvar = NODE_ROLE_INVALID;

enum node_role_t get_nodeType(void){
        if(&linkaddr_node_addr == &etc_controller) {
            nodeType_returnvar = NODE_ROLE_CONTROLLER;
            return nodeType_returnvar;
        }

    for(int i = 0; i<NUM_SENSORS; i++){
        if(&linkaddr_node_addr == &etc_sensors[i]){
            nodeType_returnvar = NODE_ROLE_SENSOR_ACTUATOR;
            return nodeType_returnvar;
        }
    }

    if (nodeType_returnvar == NODE_ROLE_INVALID)
        return nodeType_returnvar;

    //if(&linkaddr_node_addr == &etc_forwarder){
        nodeType_returnvar = NODE_ROLE_FORWARDER;
        return nodeType_returnvar;
    //}
}


/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Application */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(app_process, ev, data)
{
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);

  /* Start energest to estimate node duty cycle */
  simple_energest_start();

  printf("App: I am node %02x:%02x\n",
    linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

  static enum node_role_t nodeType;

  while(true) {

      nodeType = get_nodeType(); //implement in node allocator //implemented here, port later
    /* Controller opens connection, then waits events and data
    * coming with the callback to generate actuation commands */

   // if(linkaddr_cmp(&etc_controller, &linkaddr_node_addr)) {


      if(nodeType == NODE_ROLE_CONTROLLER){
      /* Set callbacks */
      cb.ev_cb = ev_cb;
      cb.recv_cb = recv_cb;
      cb.com_cb = NULL;

      /* Set the sensor structure */
      int i;
      for(i=0; i<NUM_SENSORS; i++) {
        linkaddr_copy(&sensor_readings[i].addr, &etc_sensors[i]);
        sensor_readings[i].reading_available = false;
        sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;
        sensor_readings[i].seqn = 0;
        sensor_readings[i].value = 0;
        sensor_readings[i].command = COMMAND_TYPE_NONE;
      }; //wrap in function?
      num_sensor_readings = 0;

      /* Open connection (builds the tree when started) */ //implement beacon+tree first
      etc_open(&etc, ETC_FIRST_CHANNEL, NODE_ROLE_CONTROLLER, &cb, etc_sensors, NUM_SENSORS);
      printf("App: Controller started\n");
    }   //starts controller module, requries event dependancies


    else if(nodeType == NODE_ROLE_SENSOR_ACTUATOR) {

      /* Check if the node is a sensor/actuator  */
      int i;
      is_sensor = false;
      for(i=0; i<NUM_SENSORS; i++) {
        if(linkaddr_cmp(&etc_sensors[i], &linkaddr_node_addr)) {
          is_sensor = true;
          prev_cmd_s.cmdtype = COMMAND_TYPE_NONE;   //set previous command to none
          /* Initialize sensed data and threshold */
          sensor_value = SENSOR_STARTING_VALUE_STEP * i;
          sensor_threshold = CONTROLLER_MAX_DIFF;

          /* Set periodic update of the sensed value */
          ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);

          /* Set callbacks */
          cb.ev_cb = NULL;      //event callback
          cb.recv_cb = NULL;    //receive callback
          cb.com_cb = com_cb;   //command callback

          /* Open connection (only the command callback is set for sensor/actuators) */
          etc_open(&etc, ETC_FIRST_CHANNEL, NODE_ROLE_SENSOR_ACTUATOR, &cb, etc_sensors, NUM_SENSORS);
          printf("App: Sensor/actuator started\n");
          break;
        }
      }
      }//starts sensor/actuator module, requries callback dependancies

      /* The node is a forwarder */
      if(nodeType == NODE_ROLE_FORWARDER) {

        /* Open connection (no callback is set for forwarders) */
        etc_open(&etc, ETC_FIRST_CHANNEL, NODE_ROLE_FORWARDER, &cb, etc_sensors, NUM_SENSORS);
        printf("App: Forwarder started\n");
      }
    }

    /* Wait for button press (node failure simulation) */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    printf("App: Simulating node failure\n");
    etc_close(&etc);
    NETSTACK_MAC.off(false);
    leds_on(LEDS_RED);

    /* Pressing again will resume normal operations */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    printf("App: Simulating node recovery\n");
    NETSTACK_MAC.on();
    leds_off(LEDS_RED);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Periodic function to update the sensed value (and trigger events) */
static void
sensor_timer_cb(void* ptr) {
  sensor_value += SENSOR_UPDATE_INCREMENT;
  etc_update(sensor_value, sensor_threshold);
  printf("Reading (%lu, %lu)\n", sensor_value, sensor_threshold);
  if(sensor_value > sensor_threshold) {
    int ret = etc_trigger(&etc, sensor_value, sensor_threshold);

    /* Logging (should not log if etc_trigger returns 0,
     * indicating that new events are currently being suppressed) */
    if(ret) {
      printf("TRIGGER [%02x:%02x, %u]\n",
        etc.event_source.u8[0], etc.event_source.u8[1],
        etc.event_seqn);
    }
  }
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);
}
/*---------------------------------------------------------------------------*/
/* Data collection reception callback.
 * The controller sets this callback to store the readings of all sensors.
 * When all readings have been collected, the controller can send commands.
 * You may send commands earlier if some data is missing after a timeout,
 * running actuation logic on the acquired data. */
static void recv_cb(const linkaddr_t *event_source, uint16_t event_seqn, const linkaddr_t *source, uint32_t value, uint32_t threshold) {

    struct event_msg_t *event = currentEvent();
    struct sensor_reading_t *sensorVal = NULL;
    struct sensor_reading_t *sensorEvent = NULL;
//    struct sensor_reading_t *sensorVal_x = NULL;
//    struct sensor_reading_t *sensorEvent_x = NULL;


  /* What if controller has not seen the event message for this collection?
   * Add proper logging! */

  int i;
  for (i=0; i<NUM_SENSORS; i++){
      if (sensorVal == NULL){ //read value here
          sensorVal = (linkaddr_cmp(event_source, &sensor_readings[i].addr) ? &sensor_readings[i] : NULL);
      }
          if(sensorVal == NULL) {
              printf("DISCARDING [%02x:%02x, %u] %02x:%02x (%lu, %lu) as failed to verify source node\n",
                     etc.event_source.u8[0], etc.event_source.u8[1],
                     etc.event_seqn,
                     source->u8[0], source->u8[1],
                     value, threshold);
              return;
          }

      if (sensorEvent == NULL){//read event here
          sensorEvent = (linkaddr_cmp(event_source, &sensor_readings[i].addr) ? &sensor_readings[i] : NULL);
      }
          if(sensorEvent == NULL) {
              printf("DISCARDING [%02x:%02x, %u] %02x:%02x (%lu, %lu) as failed to verify event source\n",
                     etc.event_source.u8[0], etc.event_source.u8[1],
                     etc.event_seqn,
                     source->u8[0], source->u8[1],
                     value, threshold);
              return;
          }

      else if (sensorVal != NULL){
          if(sensorEvent != NULL){  //go!
              break;
          }
      }
  }

  /* Add sensor reading (careful with duplicates!) */
    if (i > NUM_SENSORS){
        printf("DISCARDING [%02x:%02x, %u] %02x:%02x (%lu, %lu) as number of nodes exceeded\n",
               etc.event_source.u8[0], etc.event_source.u8[1],
               etc.event_seqn,
               source->u8[0], source->u8[1],
               value, threshold);
        return;
    }
    if (value == sensorVal.value && threshold == sensorVal.threshold){//check if same receive
        if(sensorVal.reading_available){
            printf("DISCARDING [%02x:%02x, %u] %02x:%02x (%lu, %lu) as duplicate\n",
                   etc.event_source.u8[0], etc.event_source.u8[1],
                   etc.event_seqn,
                   source->u8[0], source->u8[1],
                   value, threshold);
            return;
        }
    }

  /* Logging (based on the current event handled by the controller,
   * identified by the event_source and its sequence number);
   * in principle, this may not be the same event_source and event_seqn
   * in the callback, if the transmission was triggered by a
   * concurrent event. To match logs, the controller should
   * always use the same event_source and event_seqn for collection
   * and actuation */
bool check1 = linkaddr_cmp (event_source, &sensorEvent.addr);
bool check2 = linkaddr_cmp (event_source, &event.event_source);
  if( event_seqn != sensorEvent.seqn || !check1){ //check if event failed
      printf("Collection [%02x:%02x, %u] %02x:%02x (%lu, %lu) failed\n",
             etc.event_source.u8[0], etc.event_source.u8[1],
             etc.event_seqn,
             source->u8[0], source->u8[1],
             value, threshold);
      return;
  }
    if( event_seqn != sensorEvent.seqn || !check2){ //check if event registered
        printf("Collection [%02x:%02x, %u] %02x:%02x (%lu, %lu) not registered\n",
               etc.event_source.u8[0], etc.event_source.u8[1],
               etc.event_seqn,
               source->u8[0], source->u8[1],
               value, threshold);
        return;
    }

    //update sensor reading and read counter
    num_sensor_readings += 1;
    sensorVal.reading_available = true;
    sensorVal.value = value;
    sensorVal.threshold = threshold;
    sensorVal.command = COMMAND_TYPE_NONE;
  printf("COLLECT [%02x:%02x, %u] %02x:%02x (%lu, %lu)\n",
    etc.event_source.u8[0], etc.event_source.u8[1],
    etc.event_seqn,
    source->u8[0], source->u8[1],
    value, threshold);

  /* If all data was collected, call actuation logic */

  if(num_sensor_readings > NUM_SENSORS){ //check if all data is collected
      ctimer_stop(&collectionTimer);
      collectionTimer_cb(); //actuate
  }

}
/*---------------------------------------------------------------------------*/
/* Event detection callback; //DONE
 * This callback notifies the controller of an ongoing event dissemination.
 * After this notification, the controller waits for sensor readings.
 * The event callback should come with the event_source (the address of the
 * sensor that generated the event) and the event_seqn (a growing sequence
 * number). The logging, reporting source and sequence number, can be matched
 * with data collection logging to count how many packets, associated to this
 * event, were received. */
static void ev_cb(const linkaddr_t *event_source, uint16_t event_seqn) { //events are generated on sensor value



  struct event_msg_t *curr_ev = currentEvent();
  struct sensor_reading_t *sensorVal;


    /* Check if the event is old and discard it in that case;
   * otherwise, update the current event being handled */

    if(event_seqn != 0 && event_seqn <= sensorVal.seqn){ //old value
            printf("EVENT [%02x:%02x, %u] DISCARDED as event is old\n",
                   etc.event_source.u8[0], etc.event_source.u8[1],
                   etc.event_seqn);
            return;
        }



  /* Logging */
  if(ctimer_expired(&collectionTimer) == 0) {
      printf("EVENT [%02x:%02x, %u] DISCARDED as timer is running\n",
             etc.event_source.u8[0], etc.event_source.u8[1],
             etc.event_seqn);
      return;
  }
  /* get sensor readings */
  int i;
  for(i=0; i < NUM_SENSORS; i++){
      sensorVal = &sensor_readings[i];
      if(event_source == sensorVal.addr)    //check if address is from sensor
          break;
  }
  if(i > NUM_SENSORS)
      printf("ERROR: failed to verify sensor\n");

  sensorVal.seqn = event_seqn; //update sequence number

  //RESET
  for(i=0; i < NUM_SENSORS; i++){
      sensorVal[i].command = COMMAND_TYPE_NONE;
      sensorVal[i].reading_available = 0;
  }
  num_sensor_readings = 0;
  ctimer_set(&collectionTimer, CONTROLLER_COLLECT_WAIT, collectionTimer_cb; NULL);
}//end event callback function

/*---------------------------------------------------------------------------*/
/* Command reception callback;
 * This callback notifies the sensor/actuator of a command from the controller.
 * In this system, commands can only be of 2 types:
 * - COMMAND_TYPE_RESET:      sensed value should go to 0, and the threshold
                              back to normal;
 * - COMMAND_TYPE_THRESHOLD:  sensed value should not be modified, but the
                              threshold should be increased */
static void
com_cb(const linkaddr_t *event_source, uint16_t event_seqn, command_type_t command, uint32_t threshold) {
  /* Logging (based on the source and sequence number in the command message
   * sent by the sink, to guarantee that command transmission and
   * actuation can be matched by the analysis scripts) */

bool check3 = linkaddr_cmp(event_source, &prev_cmd_s.event_source); //check if source is same
if (check3 && prev_cmd_s.event_seqn == event_seqn && prev_cmd_s.threshold == threshold && prev_cmd_s.cmdtype && command){
    printf("duplicate command [%02x:%02x, %u] %02x:%02x discarded\n",
           event_source->u8[0], event_source->u8[1],
           event_seqn,
           linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    return;
}

  printf("ACTUATION [%02x:%02x, %u] %02x:%02x\n",
    event_source->u8[0], event_source->u8[1],
    event_seqn,
    linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

  /* Execute commands */


  if (command == COMMAND_TYPE_NONE){
      printf("COMMAND_TYPE_NONE\n");
      return;
  }
  else if(command == COMMAND_TYPE_RESET){
      printf("COMMAND_TYPE_RESET\n");
      sensor_value = 0;
      sensor_threshold = threshold;
      return;
  }
  else if(command == COMMAND_TYPE_THRESHOLD){
      printf("COMMAND_TYPE_THRESHOLD\n");
      sensor_threshold = threshold;
      return;
  }
  //if serviced, update the previous command struct with current command
  prev_cmd_s.event_source = event_source;
  prev_cmd_s.event_seqn = event_seqn;
  prev_cmd_s.cmdtype = command;
  prev_cmd_s.threshold = threshold;


}
/*---------------------------------------------------------------------------*/
/* The actuation logic to be called after sensor readings have been collected.
 * This functions checks for the steady state conditions and assigns commands
 * to all sensor/actuators that are violating them.
 * Should be called before actuation_commands(), which sends ACTUATION messages
 * based on the results of actuation_logic() */
void actuation_logic() {
  if(num_sensor_readings < 1) {
    printf("Controller: No data collected\n");
    return;
  }

  /* Debug: missing sensors */
  int i, j;
  for(i=0; i<NUM_SENSORS; i++) {
    if(!sensor_readings[i].reading_available) {
      printf("Controller: Missing %02x:%02x data\n",
        sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1]);
    }
  }

  /* Search for nodes in need of actuation */
  bool restart_check = false;
  while(true) {
    restart_check = false;

    /* Find min */
    uint32_t value_min = 0;
    for(i=0; i<NUM_SENSORS; i++) {
      if(sensor_readings[i].reading_available) {
        value_min = sensor_readings[i].value;
        break;
      }
    }
    for(; i<NUM_SENSORS; i++) {
      if(sensor_readings[i].reading_available) {
        value_min = MIN(value_min, sensor_readings[i].value);
      }
    }

    /* Check for any violation of the steady state condition,
     * and for sensors with outdated thresholds. */
    for(i=0; i<NUM_SENSORS; i++) {
      for(j=0; j<NUM_SENSORS; j++) {
        if(!sensor_readings[i].reading_available) continue;

        /* Check actuation command needed, if any;
         * case 1) the maximum difference is being exceeded;
         * case 2) the current (local) threshold of a node is being exceeded. */
        if(sensor_readings[j].reading_available &&
           (sensor_readings[i].value >= sensor_readings[j].value + CONTROLLER_MAX_DIFF
            || sensor_readings[i].threshold > CONTROLLER_MAX_THRESHOLD)) {
          sensor_readings[i].command = COMMAND_TYPE_RESET;
          sensor_readings[i].value = 0;
          sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;

          printf("Controller: Reset %02x:%02x (%lu, %lu)\n",
            sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1],
            sensor_readings[i].value, sensor_readings[i].threshold);

          /* A value was changed, restart values check */
          restart_check = true;
        }
        else if(sensor_readings[i].value > sensor_readings[i].threshold) {
          sensor_readings[i].command = COMMAND_TYPE_THRESHOLD;
          sensor_readings[i].threshold += value_min;

          printf("Controller: Update threshold %02x:%02x (%lu, %lu)\n",
            sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1],
            sensor_readings[i].value, sensor_readings[i].threshold);

          /* A value was changed, restart values check */
          restart_check = true;
        }
      }
    }
    if(!restart_check) break;
  }
}
/*---------------------------------------------------------------------------*/
/* Sends actuations for all sensors with a pending command.
 * actuation_commands() should be called after actuation_logic(), as
 * the logic sets the command for each sensor in their associated structure. */
void
actuation_commands() {
  int i;
  for(i=0; i<NUM_SENSORS; i++) {
    if(sensor_readings[i].command != COMMAND_TYPE_NONE) {
      etc_command(&etc,
        &sensor_readings[i].addr,
        sensor_readings[i].command,
        sensor_readings[i].threshold);

      /* Logging (based on the current event, expressed by source seqn) */
      printf("COMMAND [%02x:%02x, %u] %02x:%02x\n",
        etc.event_source.u8[0], etc.event_source.u8[1],
        etc.event_seqn,
        sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1]);
    }
  }
}
/*---------------------------------------------------------------------------*/
