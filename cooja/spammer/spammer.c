#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "sys/etimer.h"
#include "os/sys/log.h"
#include <stdio.h>
#include <string.h>

#define LOG_MODULE "Spammer"
#define LOG_LEVEL LOG_LEVEL_NONE

#define UDP_PORT 9999
#define SPAM_INTERVAL (CLOCK_SECOND / 100) // 10ms

static struct simple_udp_connection udp_conn;
static struct etimer spam_timer;
static uint8_t is_spamming = 0;
static uint32_t spam_count = 0;

PROCESS(spammer_process, "Network Spammer Node");
AUTOSTART_PROCESSES(&spammer_process);

/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  /* Spammer does not process received packets */
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(spammer_process, ev, data)
{
  uip_ipaddr_t dest_ipaddr;
  static char dummy_payload[64];

  PROCESS_BEGIN();

  /* Turn on LED immediately at the very start of the process */
  leds_off(LEDS_ALL);
  leds_on(LEDS_GREEN);

  /* Initialize dummy payload */
  memset(dummy_payload, 'A', sizeof(dummy_payload) - 1);
  dummy_payload[sizeof(dummy_payload) - 1] = '\0';

  /* Initialize simple UDP connection */
  simple_udp_register(&udp_conn, UDP_PORT, NULL,
                      UDP_PORT, udp_rx_callback);

  /* Set target destination to all-nodes link-local multicast address */
  uip_ip6addr(&dest_ipaddr, 0xff02, 0, 0, 0, 0, 0, 0, 0x0001);

  while(1) {
    PROCESS_YIELD();

    /* Toggle spamming when the button is released */
    if(ev == button_hal_release_event) {
      is_spamming = !is_spamming;
      if(is_spamming) {
        LOG_INFO("!!! SPAMMER ACTIVE - Flooding channel every 100ms !!!\n");
        leds_off(LEDS_ALL);
        leds_on(LEDS_RED); // Red means active spamming
        etimer_set(&spam_timer, SPAM_INTERVAL);
      } else {
        LOG_INFO("=== SPAMMER IDLE - Congestion stopped ===\n");
        leds_off(LEDS_ALL);
        leds_on(LEDS_GREEN); // Green means idle
      }
    }

    /* Send packets periodically when spamming is active */
    if(ev == PROCESS_EVENT_TIMER && data == &spam_timer) {
      if(is_spamming) {
        /* Send a burst of 5 packets back-to-back to saturate the channel */
        for(int i = 0; i < 5; i++) {
          simple_udp_sendto(&udp_conn, dummy_payload, strlen(dummy_payload), &dest_ipaddr);
        }
        spam_count += 5;
        etimer_set(&spam_timer, SPAM_INTERVAL);
      }
    }
  }

  PROCESS_END();
}
