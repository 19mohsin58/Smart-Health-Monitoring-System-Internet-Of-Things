/*
 * ============================================================================
 *               CoAP Actuator Resource Endpoint (res-health-actuator.c)
 * ============================================================================
 *
 * Objective:
 *   Handles incoming CoAP POST/PUT actuator requests to remotely update the
 *   LED indicators, alert states, and value-based reporting filters.
 *
 * Exposed Endpoint:
 *   - POST/PUT /health/actuator?mode=on|off|congestion_on|congestion_off
 * ============================================================================
 */

#include "coap-engine.h"          /* Core engine interface for CoAP resources */
#include "dev/leds.h"                 /* Visual LED hardware controls */
#include <string.h>                   /* String parsing and comparison helpers */

/* Expose global variables declared in sensor-node.c */
extern int alert_active;
extern int model_alert_active;
extern int user_alert_active;
extern int congestion_mode;

/* Log Configuration */
#include "sys/log.h"
#define LOG_MODULE "Actuator"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Forward declaration of POST/PUT callback handler */
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/* Declare and compile CoAP resource object structure */
RESOURCE(res_health_actuator,
         "title=\"Actuator control: POST/PUT mode=on|off|congestion_on|congestion_off\";rt=\"Control\"",
         NULL,
         res_post_put_handler,
         res_post_put_handler,
         NULL);

/*
 * Name: res_post_put_handler
 * Desc: Callback executed upon receiving a POST or PUT request on this resource.
 */
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *mode = NULL;
  int success = 0;

  /* Check POST/PUT body or query parameters for the 'mode' parameter key */
  if((len = coap_get_post_variable(request, "mode", &mode)) || 
     (len = coap_get_query_variable(request, "mode", &mode))) {
    
    LOG_INFO("CoAP actuator mode request: %.*s\n", (int)len, mode);

    /* Remotely force the Red Warning LED and alert state ON */
    if(strncmp(mode, "on", len) == 0) {
      user_alert_active = 1;
      alert_active = 1;
      leds_off(LEDS_ALL);
      leds_on(LEDS_RED);
      LOG_INFO("[COAP] Actuator alert turned ON (Red LED)\n");
      success = 1;
    } 
    /* Remotely clear the alert state and force the Green LED ON */
    else if(strncmp(mode, "off", len) == 0) {
      model_alert_active = 0;
      user_alert_active = 0;
      alert_active = 0;
      leds_off(LEDS_ALL);
      leds_on(LEDS_GREEN);
      LOG_INFO("[COAP] Actuator alert turned OFF (Green LED)\n");
      success = 1;
    } 
    /* Remotely enable Value-Based reporting (Mechanism 2) */
    else if(strncmp(mode, "congestion_on", len) == 0) {
      congestion_mode = 1;
      LOG_INFO("[COAP] Congestion mode enabled! Activating Value-Based reporting adaptation.\n");
      success = 1;
    } 
    /* Remotely disable Value-Based reporting (Mechanism 2) */
    else if(strncmp(mode, "congestion_off", len) == 0) {
      congestion_mode = 0;
      LOG_INFO("[COAP] Congestion mode disabled! Restoring periodic reporting.\n");
      success = 1;
    }
  }

  /* Return Status Code based on parameters match outcome */
  if(success) {
    coap_set_status_code(response, CHANGED_2_04); /* Parameter modified successfully */
  } else {
    coap_set_status_code(response, BAD_REQUEST_4_00); /* Missing or invalid parameters */
  }
}
