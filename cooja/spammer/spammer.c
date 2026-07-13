/*
 * ============================================================================
 *               High-Intensity Network Spammer Node (spammer.c)
 * ============================================================================
 *
 * Objective:
 *   Stress-testing node that generates physical radio collisions by flooding
 *   broadcast UDP traffic.
 *
 * Usage:
 *   - Button Click: Toggles active spamming state.
 *   - Green LED: Idle status.
 *   - Red LED: Active spamming status (flooding active).
 * ============================================================================
 */

#include "contiki.h"                     /* Core Contiki-NG system library */
#include "net/ipv6/simple-udp.h"         /* Lightweight UDP communication module */
#include "dev/button-hal.h"              /* Button click capture drivers */
#include "dev/leds.h"                    /* Indicator LED controls */
#include "sys/etimer.h"                  /* Non-blocking event timers */
#include "os/sys/log.h"                  /* Logging utilities */
#include <stdio.h>                       /* Standard format helper */
#include <string.h>                      /* Memory handling utilities */

#define LOG_MODULE "Spammer"
#define LOG_LEVEL LOG_LEVEL_NONE          /* Disabled to avoid USB blockages */

#define UDP_PORT 9999                    /* Target UDP port */
#define SPAM_INTERVAL (CLOCK_SECOND / 100) /* Flooding tick interval (10ms) */

/* UDP connection state reference structure */
static struct simple_udp_connection udp_conn;

/* Periodic flooding event timer */
static struct etimer spam_timer;

/* State flags */
static uint8_t is_spamming = 0;          /* Jammer status (1=Spamming, 0=Idle) */
static uint32_t spam_count = 0;          /* Total packets sent count */

/* Declare main process execution thread */
PROCESS(spammer_process, "Network Spammer Node");
AUTOSTART_PROCESSES(&spammer_process);

/*---------------------------------------------------------------------------*/
/*
 * Name: udp_rx_callback
 * Desc: Stub receiver callback. Spammer does not process received replies.
 */
static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  /* Spammer does not parse incoming messages */
}
/*---------------------------------------------------------------------------*/
/*
 * Name: spammer_process
 * Desc: Core loop for managing button events and sending UDP packet bursts.
 */
PROCESS_THREAD(spammer_process, ev, data)
{
  uip_ipaddr_t dest_ipaddr;
  static char dummy_payload[64];

  PROCESS_BEGIN();

  /* Turn on Green LED immediately on boot to declare operational state */
  leds_off(LEDS_ALL);
  leds_on(LEDS_GREEN);

  /* Build dummy junk payload to fill the 802.15.4 MTU frame */
  memset(dummy_payload, 'A', sizeof(dummy_payload) - 1);
  dummy_payload[sizeof(dummy_payload) - 1] = '\0';

  /* Register UDP endpoint socket */
  simple_udp_register(&udp_conn, UDP_PORT, NULL,
                      UDP_PORT, udp_rx_callback);

  /* Initialize multicast address pointing to all link-local nodes (ff02::1) */
  uip_ip6addr(&dest_ipaddr, 0xff02, 0, 0, 0, 0, 0, 0, 0x0001);

  while(1) {
    PROCESS_YIELD(); /* Suspend execution thread until an event is received */

    /* Toggle active state when the user presses and releases the button */
    if(ev == button_hal_release_event) {
      is_spamming = !is_spamming;
      if(is_spamming) {
        LOG_INFO("!!! SPAMMER ACTIVE !!!\n");
        leds_off(LEDS_ALL);
        leds_on(LEDS_RED); /* Red indicates active jamming */
        etimer_set(&spam_timer, SPAM_INTERVAL);
      } else {
        LOG_INFO("=== SPAMMER IDLE ===\n");
        leds_off(LEDS_ALL);
        leds_on(LEDS_GREEN); /* Green indicates idle mode */
      }
    }

    /* Send burst packets when the flooding timer fires */
    if(ev == PROCESS_EVENT_TIMER && data == &spam_timer) {
      if(is_spamming) {
        /* Send a burst of 5 packets back-to-back to saturate the radio layer */
        for(int i = 0; i < 5; i++) {
          simple_udp_sendto(&udp_conn, dummy_payload, strlen(dummy_payload), &dest_ipaddr);
        }
        spam_count += 5;
        
        /* Reload the 10ms timer */
        etimer_set(&spam_timer, SPAM_INTERVAL);
      }
    }
  }

  PROCESS_END();
}
