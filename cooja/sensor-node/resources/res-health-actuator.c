#include "coap-engine.h"
#include "dev/leds.h"
#include <string.h>

/* Extern global variables from sensor-node.c */
extern int alert_active;

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "Actuator"
#define LOG_LEVEL LOG_LEVEL_INFO

static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/* Expose POST/PUT /health/actuator */
RESOURCE(res_health_actuator,
         "title=\"Actuator control: POST/PUT mode=on|off\";rt=\"Control\"",
         NULL,
         res_post_put_handler,
         res_post_put_handler,
         NULL);

static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *mode = NULL;
  int success = 0;

  /* Check POST/PUT body or query parameters for 'mode' */
  if((len = coap_get_post_variable(request, "mode", &mode)) || 
     (len = coap_get_query_variable(request, "mode", &mode))) {
    
    LOG_INFO("CoAP actuator mode request: %.*s\n", (int)len, mode);

    if(strncmp(mode, "on", len) == 0) {
      alert_active = 1;
      leds_off(LEDS_ALL);
      leds_single_on(LEDS_RED);
      LOG_INFO("[COAP] Actuator alert turned ON (Red LED)\n");
      success = 1;
    } else if(strncmp(mode, "off", len) == 0) {
      alert_active = 0;
      leds_off(LEDS_ALL);
      leds_single_on(LEDS_GREEN);
      LOG_INFO("[COAP] Actuator alert turned OFF (Green LED)\n");
      success = 1;
    }
  }

  if(success) {
    coap_set_status_code(response, CHANGED_2_04);
  } else {
    coap_set_status_code(response, BAD_REQUEST_4_00);
  }
}
