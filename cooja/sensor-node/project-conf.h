#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Enable TCP Core Engine stack capabilities (Mandatory for MQTT) */
#define UIP_CONF_TCP 1

/* Enforce RPL-Lite Mesh Network Engine routing model activation */
#define ROUTING_CONF_RPL_LITE 1

/* Global Address Prefix Configuration matching our Host machine tunnel interface */
#define MQTT_CLIENT_CONF_BROKER_IP_ADDR "fd00::1"

#define MQTT_CLIENT_CONF_STATUS_LED 0
#define MQTT_LOG_CONF_LEVEL LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_DBG

#endif /* PROJECT_CONF_H_ */
