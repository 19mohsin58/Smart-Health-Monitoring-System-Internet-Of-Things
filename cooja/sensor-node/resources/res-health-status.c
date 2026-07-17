#include "coap-engine.h"
#include <stdio.h>
#include <string.h>

/* Extern global variables from sensor-node.c */
extern int heart_rate;
extern int alert_active;
extern int model_alert_active;

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/* Expose GET /health/status */
RESOURCE(res_health_status,
         "title=\"Health status: ?type=json\";rt=\"Status\"",
         res_get_handler,
         NULL,
         NULL,
         NULL);

static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int length = snprintf((char *)buffer, preferred_size,
                        "{\"heart_rate\":%d,\"anomaly\":%d,\"alert\":%d}",
                        heart_rate, model_alert_active, alert_active);

  coap_set_header_content_format(response, APPLICATION_JSON);
  coap_set_payload(response, buffer, length);
}
