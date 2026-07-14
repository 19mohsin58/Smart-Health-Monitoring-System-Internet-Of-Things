/*
 * ============================================================================
 *               Smart Health Sensor Node Implementation (sensor-node.c)
 * ============================================================================
 *
 * Objective:
 *   Firmware running on physical nRF52840 client dongles to simulate patient
 *   vitals, run local TinyML inference, and interact with the Border Router
 *   via CoAP and MQTT protocols.
 *
 * Key Features & Functions:
 *   - MAC-to-Patient Mapping: Configures a unique patient profile at boot.
 *   - Edge TinyML Classifier: Executes on-device Random Forest classifications.
 *   - Dynamic Backoff (Mechanism 1): Adjusts transmit intervals during alerts.
 *   - Deadband Filter (Mechanism 2): Suppresses data when channel is congested.
 *   - CoAP Server Endpoints: Exposes actuator and status diagnostic interfaces.
 *   - MQTT Publishing/Subscription: Coordinates with cloud-based brokers.
 * ============================================================================
 */

#include "contiki.h"                     /* Core Contiki-NG system library */
#include "net/routing/routing.h"         /* Mesh routing capabilities (RPL) */
#include "mqtt.h"                        /* Lightweight MQTT protocol client */
#include "net/ipv6/uip.h"                /* IPv6 networking stack driver */
#include "net/ipv6/uip-icmp6.h"          /* Control messages helper */
#include "sys/etimer.h"                  /* Event timer library for non-blocking wait */
#include "dev/button-hal.h"              /* Button Hardware Abstraction Layer API */
#include "dev/leds.h"                    /* Visual indicator hardware controls */
#include "os/sys/log.h"                  /* Print logging system macros */
#include "coap-engine.h"                 /* Constrained Application Protocol server engine */
#include "coap-blocking-api.h"          /* Synchronous CoAP transaction wrappers */
#include <string.h>                      /* String handling operations */
#include <stdio.h>                       /* Standard input/output format formatting */
#include <stdlib.h>                      /* General standard library utilities */

/*---------------------------------------------------------------------------*/
/* MQTT Topic Configuration */
#define STATUS_TOPIC       "health/node/status"     /* Telemetry publishing topic */
#define ALERT_TOPIC        "health/node/alert"      /* High-priority emergency alerts */
#define ACTUATOR_TOPIC     "health/node/actuator"   /* Received actuator command topic */
#define REGISTER_TOPIC     "health/node/register"   /* Dynamic registration handshake */

/* System Default Timings */
#define SEND_INTERVAL      (5 * CLOCK_SECOND)       /* Default reporting interval (5s) */
#define CONNECT_INTERVAL   (5 * CLOCK_SECOND)       /* Connection retry delay (5s) */

/* Data Buffers */
#define APP_BUFFER_SIZE    256                      /* Buffer size for building JSON strings */

/* Default Local IPv6 Gateway Broker Address */
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"
/*---------------------------------------------------------------------------*/

/* Declare main process execution thread */
PROCESS(sensor_node_process, "Health Sensor Node");
AUTOSTART_PROCESSES(&sensor_node_process);

/* Persistent connection metadata structure for MQTT driver */
static struct mqtt_connection conn;

/* Dedicated memory space for telemetry serialization */
static char app_buffer[APP_BUFFER_SIZE];
static char client_id[32];

/* Non-blocking event timers */
static struct etimer send_timer;
static struct etimer connect_timer;

/* Send interval duration (will change dynamically during backoff adaptation) */
static clock_time_t send_interval = SEND_INTERVAL;

/* Vitals & Adaptability State Variables */
int heart_rate = 0;              /* Simulated patient heart rate */
int alert_active = 0;            /* Local emergency alert status (1=Active) */
int model_alert_active = 0;      /* TinyML classifier alert state */
int user_alert_active = 0;       /* Physical button click alert state */
int congestion_mode = 0;         /* Value-Based suppression mode activated (1=Active) */
static uint32_t seq_id = 0;      /* Monotonically increasing sequence tracker */
static int last_sent_heart_rate = -1; /* Previous value to calculate deadband difference */
static int last_sent_anomaly = -1;    /* Previous anomaly value to monitor status changes */

#include "heart_disease_model.h" /* Inline compiled Andrea's TinyML Random Forest model */

/* 
 * Patient Physiological Feature Profile Array mapping:
 * [0: Age, 1: Sex, 2: RestingBP, 3: Cholesterol, 4: FastingBS, 5: MaxHR]
 */
static float patient_profile[6] = {55.0f, 1.0f, 120.0f, 200.0f, 0.0f, 75.0f};

/* Externally defined CoAP resource structures */
extern coap_resource_t res_health_status;
extern coap_resource_t res_health_actuator;

/* Internal status flags for network tracking */
static uint8_t mqtt_connected = 0;
static uint8_t subscribed = 0;
static uint8_t registered = 0;

/*---------------------------------------------------------------------------*/
/*
 * Name: mqtt_event_handler
 * Desc: Callback handler processing events dispatched from the Contiki MQTT module.
 */
static void
mqtt_event_handler(struct mqtt_connection *m,
                   mqtt_event_t event,
                   void *data)
{
  switch(event) {

  case MQTT_EVENT_CONNECTED:
    /* MQTT connection established; update states and turn Green LED on */
    printf("[MQTT] Connected to broker!\n");
    mqtt_connected = 1;
    leds_on(LEDS_GREEN);
    leds_off(LEDS_RED);
    break;

  case MQTT_EVENT_DISCONNECTED:
    /* Link lost; clear states, shut down green indicators, and trigger reconnect */
    printf("[MQTT] Disconnected!\n");
    mqtt_connected = 0;
    subscribed = 0;
    registered = 0;
    leds_off(LEDS_GREEN);
    break;

  case MQTT_EVENT_PUBLISH:
    /* Triggered if the node receives a direct publish from the broker */
    printf("[MQTT] Command received from cloud!\n");
    leds_on(LEDS_YELLOW);
    break;

  case MQTT_EVENT_SUBACK:
    /* Subscription confirmation packet received */
    printf("[MQTT] Subscription confirmed!\n");
    subscribed = 1;
    break;

  case MQTT_EVENT_PUBACK:
    /* Telemetry packet acknowledge confirmed */
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
/*
 * Name: publish_status
 * Desc: Packages vitals information into JSON and publishes to STATUS_TOPIC.
 */
static void
publish_status(int heart_rate, int anomaly)
{
  /* Increment packet tracker */
  seq_id++;

  /* Format parameters into serialized JSON string */
  snprintf(app_buffer, APP_BUFFER_SIZE,
    "{\"node\":\"%s\","
    "\"seq\":%lu,"
    "\"heart_rate\":%d,"
    "\"anomaly\":%d,"
    "\"alert\":%d}",
    client_id,
    (unsigned long)seq_id,
    heart_rate,
    anomaly,
    alert_active);

  /* Send packet to MQTT library */
  mqtt_status_t status = mqtt_publish(&conn, NULL,
                                      STATUS_TOPIC,
                                      (uint8_t *)app_buffer,
                                      strlen(app_buffer),
                                      MQTT_QOS_LEVEL_0,
                                      MQTT_RETAIN_OFF);

  if(status == MQTT_STATUS_OK) {
    printf("[MQTT] Published status (seq=%lu): %s\n", (unsigned long)seq_id, app_buffer);
  } else {
    printf("[MQTT] Status publish failed: %d\n", status);
  }
}
/*---------------------------------------------------------------------------*/
/*
 * Name: publish_alert
 * Desc: Sends high-priority emergency notifications to ALERT_TOPIC.
 */
static void
publish_alert(const char *type, const char *message)
{
  snprintf(app_buffer, APP_BUFFER_SIZE,
    "{\"node\":\"%s\","
    "\"type\":\"%s\","
    "\"message\":\"%s\"}",
    client_id,
    type,
    message);

  mqtt_status_t status = mqtt_publish(&conn, NULL,
                                      ALERT_TOPIC,
                                      (uint8_t *)app_buffer,
                                      strlen(app_buffer),
                                      MQTT_QOS_LEVEL_1,
                                      MQTT_RETAIN_OFF);

  if(status == MQTT_STATUS_OK) {
    printf("[MQTT] Published ALERT: %s\n", app_buffer);
  } else {
    printf("[MQTT] ALERT publish failed: %d\n", status);
  }
}
/*---------------------------------------------------------------------------*/
/*
 * Name: publish_registration
 * Desc: Handshake registration packet to declare device type and metric parameters.
 */
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
/*
 * Name: sensor_node_process
 * Desc: Core execution loop governing hardware events, timers, and protocols.
 */
PROCESS_THREAD(sensor_node_process, ev, data)
{
  button_hal_button_t *btn;

  PROCESS_BEGIN();

  printf("====================================\n");
  printf("  Smart Health Node Started\n");
  printf("====================================\n");

  /* Extract device MAC address for dynamic client configuration mapping */
  snprintf(client_id, sizeof(client_id), "health-node-%04x", 
           (unsigned int)(linkaddr_node_addr.u8[LINKADDR_SIZE - 2] << 8 | 
                          linkaddr_node_addr.u8[LINKADDR_SIZE - 1]));

  /* Parse MAC suffix to generate patient risk profiles */
  uint16_t id_val = (linkaddr_node_addr.u8[LINKADDR_SIZE - 2] << 8 | 
                     linkaddr_node_addr.u8[LINKADDR_SIZE - 1]);

  /* Profile Customization mappings based on MAC addresses */
  if (id_val == 0x0002 || id_val == 0xd8c7) {
    /* Patient 2: Elderly High Risk Male (Always Red / Anomaly) */
    patient_profile[0] = 72.0f;  /* Age */
    patient_profile[1] = 1.0f;   /* Sex (1=Male) */
    patient_profile[2] = 150.0f; /* Resting BP */
    patient_profile[3] = 270.0f; /* Cholesterol */
    patient_profile[4] = 1.0f;   /* Fasting BS (1=High) */
  } else if (id_val == 0x0003 || id_val == 0x92a7) {
    /* Patient 3: Dynamic Young Male (Toggles at 110 BPM) */
    patient_profile[0] = 30.0f;  /* Age */
    patient_profile[1] = 1.0f;   /* Sex (1=Male) */
    patient_profile[2] = 120.0f; /* Resting BP */
    patient_profile[3] = 220.0f; /* Cholesterol */
    patient_profile[4] = 0.0f;   /* Fasting BS */
  } else if (id_val == 0x0004 || id_val == 0x4d7c) {
    /* Patient 4: Dynamic Young Male (Toggles at 110 BPM) */
    patient_profile[0] = 30.0f;  /* Age */
    patient_profile[1] = 1.0f;   /* Sex (1=Male) */
    patient_profile[2] = 120.0f; /* Resting BP */
    patient_profile[3] = 220.0f; /* Cholesterol */
    patient_profile[4] = 0.0f;   /* Fasting BS */
  } else {
    /* Patient 5 / Default: Dynamic Young Female (Matches f869, Toggles at 106 BPM) */
    patient_profile[0] = 30.0f;  /* Age */
    patient_profile[1] = 0.0f;   /* Sex (0=Female) */
    patient_profile[2] = 150.0f; /* Resting BP */
    patient_profile[3] = 260.0f; /* Cholesterol */
    patient_profile[4] = 0.0f;   /* Fasting BS */
  }

  /* Safe Integer Cast parsing to avoid floats in libc printf blocks */
  printf("[PROFILE] Mapping assigned: Age %d, Sex %d, BP %d, Chol %d, FastingBS %d\n",
         (int)patient_profile[0], (int)patient_profile[1], 
         (int)patient_profile[2], (int)patient_profile[3], 
         (int)patient_profile[4]);

  /* Start CoAP server engine */
  coap_engine_init();
  
  /* Register status and actuator endpoints */
  coap_activate_resource(&res_health_status, "health/status");
  coap_activate_resource(&res_health_actuator, "health/actuator");

  /* Setup MQTT client structures */
  mqtt_register(&conn, &sensor_node_process, client_id, mqtt_event_handler, APP_BUFFER_SIZE);

  /* Set initial non-blocking connection poll timer */
  etimer_set(&connect_timer, CONNECT_INTERVAL);

  while(1) {
    PROCESS_YIELD(); /* Suspend thread execution until an event is posted */

    /* ========================================================
     * SHORT PRESS INTERACTION — Manual Alert Activation
     * ======================================================== */
    if(ev == button_hal_release_event) {
      user_alert_active = !user_alert_active;
      alert_active = (model_alert_active || user_alert_active);
      if(alert_active) {
        leds_off(LEDS_ALL);
        leds_on(LEDS_RED);
        printf("[BUTTON] Manual alert activated!\n");
        publish_alert("EMERGENCY", "Patient pressed emergency button!");
      } else {
        leds_off(LEDS_ALL);
        leds_on(LEDS_GREEN);
        printf("[BUTTON] Manual alert deactivated.\n");
      }
    }

    /* ========================================================
     * LONG PRESS INTERACTION — Force Override Safety Reset
     * ======================================================== */
    else if(ev == button_hal_periodic_event) {
      btn = (button_hal_button_t *)data;
      if(btn->press_duration_seconds >= 3) {
        user_alert_active = 0;
        model_alert_active = 0;
        alert_active = 0;
        leds_off(LEDS_ALL);
        leds_on(LEDS_GREEN);
        printf("[LONG PRESS] Safety system force reset executed.\n");
      }
    }

    /* ========================================================
     * MQTT UPDATE EVENT — Handle Network State Transitions
     * ======================================================== */
    else if(ev == mqtt_update_event) {
      if(mqtt_connected) {
        if(!subscribed) {
          if(conn.out_buffer_sent && !conn.out_queue_full) {
            mqtt_status_t status = mqtt_subscribe(&conn, NULL, ACTUATOR_TOPIC, MQTT_QOS_LEVEL_0);
            if(status == MQTT_STATUS_OK) {
              printf("[MQTT] Subscription packet enqueued: %s\n", ACTUATOR_TOPIC);
            }
          }
        } else if(!registered) {
          if(!conn.out_queue_full) {
            publish_registration();
            if(registered) {
              etimer_set(&send_timer, send_interval);
            }
          }
        }
      } else {
        subscribed = 0;
        registered = 0;
        etimer_set(&connect_timer, CONNECT_INTERVAL);
      }
    }

    /* ========================================================
     * CONNECT TIMER LOOP — Establish Connection to Broker
     * ======================================================== */
    else if(ev == PROCESS_EVENT_TIMER && data == &connect_timer) {
      if(!mqtt_connected) {
        if(NETSTACK_ROUTING.node_is_reachable()) {
          printf("[NET] Network reachable! Connecting to MQTT broker...\n");
          mqtt_status_t status = mqtt_connect(&conn,
                                              MQTT_CLIENT_BROKER_IP_ADDR,
                                              1883, 
                                              60,
                                              MQTT_CLEAN_SESSION_ON);
          if(status != MQTT_STATUS_OK) {
            etimer_set(&connect_timer, CONNECT_INTERVAL);
          }
        } else {
          etimer_set(&connect_timer, CONNECT_INTERVAL);
        }
      }
    }

    /* ========================================================
     * SEND TIMER LOOP — Read Vitals, Run TinyML, and Publish
     * ======================================================== */
    else if(ev == PROCESS_EVENT_TIMER && data == &send_timer) {

      /* Simulate heart rate variance */
      heart_rate = 60 + (rand() % 60);
      printf("\n--- Vitals processing metric: %d BPM ---\n", heart_rate);

      /* Bind dynamic MaxHR feature */
      patient_profile[5] = (float)heart_rate;

      /* Run TinyML inference classification */
      float ml_output[2] = {0.0f, 0.0f};
      score(patient_profile, ml_output);
      int current_anomaly = (ml_output[1] >= 0.5f) ? 1 : 0;

      printf("TinyML Anomaly Prob: %d%%\n", (int)(ml_output[1] * 100.0f));

      if(current_anomaly == 1) {
        model_alert_active = 1;
        printf("CLASSIFICATION STATUS: *** ANOMALY DETECTED ***\n");
      } else {
        model_alert_active = 0;
        printf("CLASSIFICATION STATUS: NORMAL\n");
      }

      /* Re-evaluate overall alert status via OR logic */
      alert_active = (model_alert_active || user_alert_active);

      if(alert_active) {
        /* Emergency alarm active; turn Red LED on */
        leds_off(LEDS_ALL);
        leds_on(LEDS_RED);
      } else {
        /* Safe environment; turn Green LED on */
        leds_off(LEDS_ALL);
        leds_on(LEDS_GREEN);
      }

      /* 
       * Rate Adaptation (Mechanism 1):
       * - If patient manually triggered alert (user_alert_active == 1), we MUST monitor high frequency (5s).
       * - If it's a model-only anomaly (model_alert_active == 1), we back off to 20s.
       * - Otherwise, nominal 5s.
       */
      if(user_alert_active) {
        send_interval = SEND_INTERVAL;
      } else if(model_alert_active) {
        send_interval = 20 * CLOCK_SECOND;
      } else {
        send_interval = SEND_INTERVAL;
      }

      /* Transmit telemetry based on congestion conditions */
      if(mqtt_connected) {
        /* Value-Based Suppression (Mechanism 2) deadband evaluation */
        if(congestion_mode && last_sent_heart_rate != -1 && last_sent_anomaly == current_anomaly) {
          int diff = abs(heart_rate - last_sent_heart_rate);
          if(diff < 4) {
            /* Vitals stable during congestion; suppress packet transmission */
            printf("[ADAPTATION 2] Value stable (diff=%d). Suppressing publish.\n", diff);
          } else {
            publish_status(heart_rate, current_anomaly);
            last_sent_heart_rate = heart_rate;
            last_sent_anomaly = current_anomaly;
          }
        } else {
          publish_status(heart_rate, current_anomaly);
          last_sent_heart_rate = heart_rate;
          last_sent_anomaly = current_anomaly;
        }
      }

      /* Reload event timer */
      etimer_set(&send_timer, send_interval);
    }
  }

  PROCESS_END();
}
