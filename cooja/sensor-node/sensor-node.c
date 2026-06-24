/*
 * Smart Health Sensor Node
 * - Simulates heart rate sensor
 * - Detects anomalies locally
 * - Publishes data via MQTT
 * - Button = patient emergency alert
 * - LEDs = status indicators
 */

#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "sys/etimer.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*/
/* MQTT topics */
#define STATUS_TOPIC       "health/node/status"
#define ALERT_TOPIC        "health/node/alert"
#define ACTUATOR_TOPIC     "health/node/actuator"
#define REGISTER_TOPIC     "health/node/register"

/* Timing */
#define SEND_INTERVAL      (5 * CLOCK_SECOND)
#define CONNECT_INTERVAL   (5 * CLOCK_SECOND)

/* Buffer sizes */
#define APP_BUFFER_SIZE    256

/* Broker IP Address Configuration */
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"
/*---------------------------------------------------------------------------*/

PROCESS(sensor_node_process, "Health Sensor Node");
AUTOSTART_PROCESSES(&sensor_node_process);

/* MQTT connection structure */
static struct mqtt_connection conn;

/* Buffers */
static char app_buffer[APP_BUFFER_SIZE];
static char client_id[32];

/* Timers */
static struct etimer send_timer;
static struct etimer connect_timer;

/* Global telemetry variables accessed by CoAP resources */
int heart_rate = 0;
int alert_active = 0;
int manual_alert = 0;

/* CoAP client endpoint and configuration */
#define COAP_SERVER_EP "coap://[fd00::1]:5683"
static coap_endpoint_t server_ep;
static uint8_t coap_registered = 0;

/* External CoAP resources definitions */
extern coap_resource_t res_health_status;
extern coap_resource_t res_health_actuator;

/* State variables */
static uint8_t mqtt_connected = 0;
static uint8_t subscribed = 0;
static uint8_t registered = 0;

/* Send interval — will change dynamically during adaptation */
static clock_time_t send_interval = SEND_INTERVAL;

/*---------------------------------------------------------------------------*/
static void
mqtt_event_handler(struct mqtt_connection *m,
                   mqtt_event_t event,
                   void *data)
{
  switch(event) {

  case MQTT_EVENT_CONNECTED:
    printf("[MQTT] Connected to broker!\n");
    mqtt_connected = 1;
    leds_single_on(LEDS_GREEN);
    leds_single_off(LEDS_RED);
    break;

  case MQTT_EVENT_DISCONNECTED:
    printf("[MQTT] Disconnected!\n");
    mqtt_connected = 0;
    subscribed = 0;
    registered = 0;
    leds_single_off(LEDS_GREEN);
    break;

  case MQTT_EVENT_PUBLISH:
    /* Command received from cloud broker */
    printf("[MQTT] Command received from cloud!\n");
    leds_single_on(LEDS_YELLOW);
    break;

  case MQTT_EVENT_SUBACK:
    printf("[MQTT] Subscription confirmed!\n");
    subscribed = 1;
    break;

  case MQTT_EVENT_PUBACK:
    printf("[MQTT] Publish confirmed!\n");
    break;

  case MQTT_EVENT_ERROR:
    printf("[MQTT] Event Error: Connection failed or encountered issue!\n");
    break;

  case MQTT_EVENT_PROTOCOL_ERROR:
    printf("[MQTT] Protocol Error occurred!\n");
    break;

  case MQTT_EVENT_CONNECTION_REFUSED_ERROR:
    printf("[MQTT] Connection Refused by broker!\n");
    break;

  case MQTT_EVENT_DNS_ERROR:
    printf("[MQTT] DNS Resolution Error!\n");
    break;

  case MQTT_EVENT_NOT_IMPLEMENTED_ERROR:
    printf("[MQTT] Not Implemented Error!\n");
    break;

  default:
    printf("[MQTT] Received unhandled event: %d\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
static void
publish_status(int heart_rate, int anomaly)
{
  /* Format payload directly into a JSON format string */
  snprintf(app_buffer, APP_BUFFER_SIZE,
    "{\"node\":\"%s\","
    "\"heart_rate\":%d,"
    "\"anomaly\":%d,"
    "\"alert\":%d}",
    client_id,
    heart_rate,
    anomaly,
    alert_active);

  mqtt_status_t status = mqtt_publish(&conn, NULL,
                                      STATUS_TOPIC,
                                      (uint8_t *)app_buffer,
                                      strlen(app_buffer),
                                      MQTT_QOS_LEVEL_0,
                                      MQTT_RETAIN_OFF);

  if(status == MQTT_STATUS_OK) {
    printf("[MQTT] Published status: %s\n", app_buffer);
  } else {
    printf("[MQTT] Status publish failed: %d\n", status);
  }
}
/*---------------------------------------------------------------------------*/
static void
publish_alert(void)
{
  snprintf(app_buffer, APP_BUFFER_SIZE,
    "{\"node\":\"%s\","
    "\"type\":\"EMERGENCY\","
    "\"message\":\"Patient pressed button!\"}",
    client_id);

  mqtt_status_t status = mqtt_publish(&conn, NULL,
                                      ALERT_TOPIC,
                                      (uint8_t *)app_buffer,
                                      strlen(app_buffer),
                                      MQTT_QOS_LEVEL_0,
                                      MQTT_RETAIN_OFF);

  if(status == MQTT_STATUS_OK) {
    printf("[MQTT] ALERT Published!\n");
  } else {
    printf("[MQTT] ALERT publish failed: %d\n", status);
  }
}
/*---------------------------------------------------------------------------*/
static void
publish_registration(void)
{
  snprintf(app_buffer, APP_BUFFER_SIZE,
    "{\"node\":\"%s\","
    "\"type\":\"sensor\","
    "\"domain\":\"smart-health\","
    "\"sensor\":\"heart-rate\"}",
    client_id);

  mqtt_status_t status = mqtt_publish(&conn, NULL,
                                      REGISTER_TOPIC,
                                      (uint8_t *)app_buffer,
                                      strlen(app_buffer),
                                      MQTT_QOS_LEVEL_0,
                                      MQTT_RETAIN_OFF);

  if(status == MQTT_STATUS_OK) {
    printf("[MQTT] Registered with broker: %s\n", app_buffer);
    registered = 1;
  } else {
    printf("[MQTT] Registration publish failed: %d\n", status);
  }
}
/*---------------------------------------------------------------------------*/
static void
coap_registration_handler(coap_message_t *response)
{
  if(response == NULL) {
    printf("[COAP] Registration request timed out!\n");
    return;
  }
  printf("[COAP] Registered with Cloud Server successfully!\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sensor_node_process, ev, data)
{
  button_hal_button_t *btn;

  PROCESS_BEGIN();

  printf("====================================\n");
  printf("  Smart Health Node Started\n");
  printf("====================================\n");

  /* Generate uniquely mapped client ID using node link layer identification */
  snprintf(client_id, sizeof(client_id),
           "health-node-%02x%02x",
           linkaddr_node_addr.u8[6],
           linkaddr_node_addr.u8[7]);
  printf("Client ID: %s\n", client_id);

  /* Clear active hardware indicators initially */
  leds_off(LEDS_ALL);
  leds_single_on(LEDS_GREEN);
  printf("[LED] GREEN ON - Node initialized\n");

  /* Initialize system MQTT connection registry parameters */
  mqtt_register(&conn, &sensor_node_process,
                client_id, mqtt_event_handler,
                APP_BUFFER_SIZE);

  /* Activate CoAP Resources */
  coap_activate_resource(&res_health_status, "health/status");
  coap_activate_resource(&res_health_actuator, "health/actuator");
  printf("[COAP] Resources activated: /health/status, /health/actuator\n");

  /* Wait for network initialization */
  printf("[NET] Waiting for network deployment layers...\n");
  etimer_set(&connect_timer, CONNECT_INTERVAL);

  /* Core Execution Event Loop Monitoring */
  while(1) {
    PROCESS_YIELD();

    /* ==============================================
     * BUTTON PRESS LAYER (Visual Event Feedback)
     * ============================================== */
    if(ev == button_hal_press_event) {
      leds_single_on(LEDS_YELLOW);
      printf("\n[BUTTON] Patient pressing interaction button...\n");
    }

    /* ==============================================
     * BUTTON RELEASE LAYER — Manual Alarm Trigger
     * ============================================== */
    else if(ev == button_hal_release_event) {
      leds_single_off(LEDS_YELLOW);

      if(alert_active == 0) {
        alert_active = 1;
        manual_alert = 1;
        leds_off(LEDS_ALL);
        leds_single_on(LEDS_RED);
        printf("[EMERGENCY] Patient manually triggered alert sequence!\n");
        printf("[LED] RED ON - Emergency State Active!\n");

        if(mqtt_connected) {
          publish_alert();
        }
      } else {
        alert_active = 0;
        manual_alert = 0;
        leds_off(LEDS_ALL);
        leds_single_on(LEDS_GREEN);
        printf("[BUTTON] Alert state manually cancelled by operator.\n");
        printf("[LED] GREEN ON - Standard Monitoring Context\n");
      }
    }

    /* ==============================================
     * LONG PRESS INTERACTION — Overriding Faults
     * ============================================== */
    else if(ev == button_hal_periodic_event) {
      btn = (button_hal_button_t *)data;
      if(btn->press_duration_seconds >= 3) {
        alert_active = 0;
        manual_alert = 0;
        leds_off(LEDS_ALL);
        leds_single_on(LEDS_GREEN);
        printf("[LONG PRESS] Safety system force reset executed successfully.\n");
        printf("[LED] GREEN ON - System parameters normalized\n");
      }
    }

    /* ==============================================
     * MQTT UPDATE EVENT — Handle connection changes
     * ============================================== */
    else if(ev == mqtt_update_event) {
      if(mqtt_connected) {
        if(!subscribed) {
          if(conn.out_buffer_sent && !conn.out_queue_full) {
            mqtt_status_t status = mqtt_subscribe(&conn, NULL, ACTUATOR_TOPIC, MQTT_QOS_LEVEL_0);
            if(status == MQTT_STATUS_OK) {
              printf("[MQTT] Subscription packet enqueued for: %s\n", ACTUATOR_TOPIC);
            } else {
              printf("[MQTT] Subscription initiation failed with status: %d\n", status);
            }
          } else {
            printf("[MQTT] Output buffer or queue busy. Postponing subscription...\n");
          }
        } else if(!registered) {
          if(!conn.out_queue_full) {
            publish_registration();
            if(registered) {
              etimer_set(&send_timer, send_interval);
            }
          } else {
            printf("[MQTT] Queue busy. Postponing registration...\n");
          }
        }
      } else {
        /* Connection lost or failed to connect, retry */
        subscribed = 0;
        registered = 0;
        printf("[MQTT] Status update: Not connected. Retrying in %d seconds...\n", (int)(CONNECT_INTERVAL / CLOCK_SECOND));
        etimer_set(&connect_timer, CONNECT_INTERVAL);
      }
    }

    /* ==============================================
     * CONNECT TIMER LOOP — Verification & Connection
     * ============================================== */
    else if(ev == PROCESS_EVENT_TIMER && data == &connect_timer) {

      if(!coap_registered) {
        if(NETSTACK_ROUTING.node_is_reachable()) {
          printf("[NET] Global RPL Network reachable! Initiating CoAP registration...\n");
          
          coap_endpoint_parse(COAP_SERVER_EP, strlen(COAP_SERVER_EP), &server_ep);
          
          static coap_message_t coap_req[1];
          coap_init_message(coap_req, COAP_TYPE_CON, COAP_POST, 0);
          coap_set_header_uri_path(coap_req, "register");
          
          snprintf(app_buffer, APP_BUFFER_SIZE,
            "{\"node\":\"%s\","
            "\"type\":\"sensor\","
            "\"domain\":\"smart-health\","
            "\"sensor\":\"heart-rate\","
            "\"protocol\":\"coap\"}",
            client_id);
          coap_set_payload(coap_req, (uint8_t *)app_buffer, strlen(app_buffer));
          
          printf("[COAP] Sending registration to Cloud Server...\n");
          COAP_BLOCKING_REQUEST(&server_ep, coap_req, coap_registration_handler);
          
          coap_registered = 1;
          
          /* Instantly trigger connect timer again to start MQTT */
          etimer_set(&connect_timer, 0);
        } else {
          printf("[NET] Routing path not ready yet. Retrying network diagnostics...\n");
          etimer_set(&connect_timer, CONNECT_INTERVAL);
        }
      }
      else if(!mqtt_connected) {
        if(NETSTACK_ROUTING.node_is_reachable()) {
          printf("[NET] Global RPL Network reachable! Establishing connection to MQTT broker...\n");
          
          mqtt_status_t status = mqtt_connect(&conn,
                                              MQTT_CLIENT_BROKER_IP_ADDR,
                                              1883, 
                                              60,
                                              MQTT_CLEAN_SESSION_ON);
          if(status != MQTT_STATUS_OK) {
            printf("[MQTT] Connection initiation failed with status: %d\n", status);
            etimer_set(&connect_timer, CONNECT_INTERVAL);
          }
        } else {
          printf("[NET] Routing path not ready yet. Retrying network diagnostics...\n");
          etimer_set(&connect_timer, CONNECT_INTERVAL);
        }
      }
    }

    /* ==============================================
     * SEND TIMER LOOP — Continuous Evaluation
     * ============================================== */
    else if(ev == PROCESS_EVENT_TIMER && data == &send_timer) {

      /* Simulate vital reading variance constraints */
      heart_rate = 60 + (rand() % 60);

      printf("\n--- Telemetry Metrics Node Log ---\n");
      printf("Heart Rate Processing Metric : %d BPM\n", heart_rate);

      if(heart_rate > 100) {
        /* LOCAL ANOMALY CLASSIFICATION TRIGGERED */
        alert_active = 1;
        leds_off(LEDS_ALL);
        leds_single_on(LEDS_RED);
        printf("CLASSIFICATION STATUS        : *** ANOMALY CLASSIFIED ***\n");
        printf("[LED] RED ON - High Priority Mode\n");

        /* MANDATORY ADAPTIVE MECHANISM 1: Rate Backoff modification */
        send_interval = 20 * CLOCK_SECOND;
        printf("[ADAPTATION] Local network congestion risk mitigation. Throttle send rate to: 20s\n");

      } else {
        /* NORMAL HEART RATE DETECTED */
        if(manual_alert == 1) {
          /* Keep alert active and Red LED on due to manual override */
          alert_active = 1;
          leds_off(LEDS_ALL);
          leds_single_on(LEDS_RED);
          printf("CLASSIFICATION STATUS        : BALANCED OPERATION (MANUAL ALERT ACTIVE)\n");
          printf("[LED] RED ON (Manual Alert Latch)\n");
          
          /* Keep congestion backoff rate since emergency alert is active */
          send_interval = 20 * CLOCK_SECOND;
        } else {
          /* NORMAL STATE RESTORED */
          alert_active = 0;
          leds_off(LEDS_ALL);
          leds_single_on(LEDS_GREEN);
          printf("CLASSIFICATION STATUS        : BALANCED OPERATIONAL ENVIRONMENT\n");
          printf("[LED] GREEN ON\n");

          /* Revert to target nominal operation rate limits */
          send_interval = SEND_INTERVAL;
        }
      }

      /* Forward tracking update states upstream */
      if(mqtt_connected) {
        publish_status(heart_rate, (heart_rate > 100) ? 1 : 0);
      }

      etimer_set(&send_timer, send_interval);
    }
  }

  PROCESS_END();
}
