/*
 * Wake-up Radio RDC layer implementation that uses framer for headers +
 * CSMA MAC layer
 * \authors
 *         Timofei Istomin <timofei.istomin@unitn.it>
 *         Rajeev Piyare   <piyare@fbk.eu>
 */

#include "net/mac/mac-sequence.h"
#include "net/mac/wurrdc.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include "wur.h"
#include "net/rime/rimestats.h"
#include <string.h>
#include <stdio.h>
#include "dev/leds.h"
#include "sys/rtimer.h"  

#if CONTIKI_TARGET_COOJA
#include "lib/simEnvChange.h"
#endif /* CONTIKI_TARGET_COOJA */


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef WURRDC_CONF_ADDRESS_FILTER
#define WURRDC_ADDRESS_FILTER WURRDC_CONF_ADDRESS_FILTER
#else
#define WURRDC_ADDRESS_FILTER 1
#endif /* WURRDC_CONF_ADDRESS_FILTER */

#ifndef WURRDC_802154_AUTOACK
#ifdef WURRDC_CONF_802154_AUTOACK
#define WURRDC_802154_AUTOACK WURRDC_CONF_802154_AUTOACK
#else
#define WURRDC_802154_AUTOACK 0
#endif /* WURRDC_CONF_802154_AUTOACK */
#endif /* WURRDC_802154_AUTOACK */

#ifndef WURRDC_802154_AUTOACK_HW
#ifdef WURRDC_CONF_802154_AUTOACK_HW
#define WURRDC_802154_AUTOACK_HW WURRDC_CONF_802154_AUTOACK_HW
#else
#define WURRDC_802154_AUTOACK_HW 0
#endif /* WURRDC_CONF_802154_AUTOACK_HW */
#endif /* WURRDC_802154_AUTOACK_HW */

#if WURRDC_802154_AUTOACK
#include "sys/rtimer.h"
#include "dev/watchdog.h"

#ifdef WURRDC_CONF_ACK_WAIT_TIME
#define ACK_WAIT_TIME WURRDC_CONF_ACK_WAIT_TIME
#else /* WURRDC_CONF_ACK_WAIT_TIME */
#define ACK_WAIT_TIME                      RTIMER_SECOND / 2500
#endif /* WURRDC_CONF_ACK_WAIT_TIME */
#ifdef WURRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#define AFTER_ACK_DETECTED_WAIT_TIME WURRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#else /* WURRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME */
#define AFTER_ACK_DETECTED_WAIT_TIME       RTIMER_SECOND / 1500
#endif /* WURRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME */
#endif /* WURRDC_802154_AUTOACK */

#ifdef WURRDC_CONF_SEND_802154_ACK
#define WURRDC_SEND_802154_ACK WURRDC_CONF_SEND_802154_ACK
#else /* WURRDC_CONF_SEND_802154_ACK */
#define WURRDC_SEND_802154_ACK 0
#endif /* WURRDC_CONF_SEND_802154_ACK */

#if WURRDC_SEND_802154_ACK
#include "net/mac/frame802154.h"
#endif /* WURRDC_SEND_802154_ACK */

#define ACK_LEN 3

uint8_t WUR_RX_LENGTH;
uint8_t WUR_RX_BUFFER[LINKADDR_SIZE];  //RX buffer for the WUR address

uint8_t WUR_TX_LENGTH;
uint8_t WUR_TX_BUFFER[LINKADDR_SIZE];  //TX buffer for the WUR address

/*---------------------------------------------------------------------------*/
static void
on(void)
{
    NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
static void
off(void)
{
    NETSTACK_RADIO.off();
}
/*---------------------------------------------------------------------------*/

static int
send_one_packet(mac_callback_t sent, void *ptr)
{
  int ret;
  int last_sent_ok = 0;
  
  linkaddr_copy((linkaddr_t*)&WUR_TX_BUFFER, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
  WUR_TX_LENGTH = LINKADDR_SIZE;
  
  wur_set_tx(); 	//send the Wake-up trigger signal, high GPIO
  clock_delay(100);	
  wur_clear_tx(); 	//clear the signal, low GPIO
  clock_delay(1000);//delay to synchronize with the receiver 
  on(); 			// turn on the radio to send the data packet

  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);

#if WURRDC_802154_AUTOACK || WURRDC_802154_AUTOACK_HW
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);
#endif /* WURRDC_802154_AUTOACK || WURRDC_802154_AUTOACK_HW */

  if(NETSTACK_FRAMER.create() < 0) {
    /* Failed to allocate space for headers */
    PRINTF("wurrdc: send failed, too large header\n");
    ret = MAC_TX_ERR_FATAL;
  } else {
#if WURRDC_802154_AUTOACK
    int is_broadcast;
    uint8_t dsn;
    dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

    NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());

    is_broadcast = packetbuf_holds_broadcast();

    if(NETSTACK_RADIO.receiving_packet() ||
       (!is_broadcast && NETSTACK_RADIO.pending_packet())) {

      /* Currently receiving a packet over air or the radio has
         already received a packet that needs to be read before
         sending with auto ack. */
      ret = MAC_TX_COLLISION;
      off();/*****************************/
      //printf("MAC_TX_COLLISION\n");
    } else {
      if(!is_broadcast) {
        RIMESTATS_ADD(reliabletx);
      }

      switch(NETSTACK_RADIO.transmit(packetbuf_totlen())) {
      case RADIO_TX_OK:
        if(is_broadcast) {
          off();
          ret = MAC_TX_OK;
        } else {
          rtimer_clock_t wt;
          /* Check for ack */
          wt = RTIMER_NOW();
          watchdog_periodic();
          while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + ACK_WAIT_TIME)) {
#if CONTIKI_TARGET_COOJA
            simProcessRunValue = 1;
            cooja_mt_yield();
#endif /* CONTIKI_TARGET_COOJA */
          }

            clock_delay(100);/*(2.83*100)us Seems sufficient enough to Rx ACK*/
            off();/****************************/
            ret = MAC_TX_NOACK;
            //printf("NOACK\n");
          if(!is_broadcast && (NETSTACK_RADIO.receiving_packet() ||
             NETSTACK_RADIO.pending_packet() ||
             NETSTACK_RADIO.channel_clear() == 0)) {
            int len;
            uint8_t ackbuf[ACK_LEN];
            if(AFTER_ACK_DETECTED_WAIT_TIME > 0) {
              wt = RTIMER_NOW();
              watchdog_periodic();
              while(RTIMER_CLOCK_LT(RTIMER_NOW(),
                                    wt + AFTER_ACK_DETECTED_WAIT_TIME)) {
      #if CONTIKI_TARGET_COOJA
                  simProcessRunValue = 1;
                  cooja_mt_yield();
      #endif /* CONTIKI_TARGET_COOJA */
              }
            }

            if(NETSTACK_RADIO.pending_packet()) {
              len = NETSTACK_RADIO.read(ackbuf, ACK_LEN);
              if(len == ACK_LEN && ackbuf[2] == dsn) {
                /* Ack received */
                RIMESTATS_ADD(ackrx);
                ret = MAC_TX_OK;
                //printf("TX_OK\n");
              } else {
                /* Not an ack or ack not for us: collision */
                ret = MAC_TX_COLLISION;
              }
            }
          } else {
	    PRINTF("wurrdc tx noack\n");
	  }
        }
        break;
      case RADIO_TX_COLLISION:
        ret = MAC_TX_COLLISION;
        off();
        break;
      default:
        ret = MAC_TX_ERR;
        off();
        break;
      }
    }
#else /* ! WURRDC_802154_AUTOACK */

    switch(NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen())) { 
    case RADIO_TX_OK:
      ret = MAC_TX_OK;
      break;
    case RADIO_TX_COLLISION:
      ret = MAC_TX_COLLISION;
      break;
    case RADIO_TX_NOACK:
      ret = MAC_TX_NOACK;
      break;
    default:
      ret = MAC_TX_ERR;
      break;
    }
#endif /* ! WURRDC_802154_AUTOACK */
  }
  if(ret == MAC_TX_OK) {
    last_sent_ok = 1;
  }
  mac_call_sent_callback(sent, ptr, ret, 1);

  return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
  send_one_packet(sent, ptr);
}

/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
  while(buf_list != NULL) {
    /* We backup the next pointer, as it may be nullified by
     * mac_call_sent_callback() */
    struct rdc_buf_list *next = buf_list->next;
    int last_sent_ok;

    queuebuf_to_packetbuf(buf_list->buf);

    last_sent_ok = send_one_packet(sent, ptr);

    /* If packet transmission was not successful, we should back off and let
     * upper layers retransmit, rather than potentially sending out-of-order
     * packet fragments. */
    if(!last_sent_ok) {
      return;
    }
    buf_list = next;
  }
}

/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{

#if WURRDC_SEND_802154_ACK
  int original_datalen;
  uint8_t *original_dataptr;

  original_datalen = packetbuf_datalen();
  original_dataptr = packetbuf_dataptr();
#endif

#if WURRDC_802154_AUTOACK
  if(packetbuf_datalen() == ACK_LEN) {
  	    /* Ignore ack packets */
    PRINTF("wurrdc: ignored ack\n"); 
  } else
#endif /* WURRDC_802154_AUTOACK */
  if(NETSTACK_FRAMER.parse() < 0) {
    PRINTF("wurrdc: failed to parse %u\n", packetbuf_datalen());
#if WURRDC_ADDRESS_FILTER
  } else if(!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                         &linkaddr_node_addr) &&
            !packetbuf_holds_broadcast()) {
    PRINTF("wurrdc: not for us\n");
#endif /* WURRDC_ADDRESS_FILTER */
  } else {
    int duplicate = 0;

#if WURRDC_802154_AUTOACK || WURRDC_802154_AUTOACK_HW
#if RDC_WITH_DUPLICATE_DETECTION
    /* Check for duplicate packet. */
    duplicate = mac_sequence_is_duplicate();
    if(duplicate) {
      /* Drop the packet. */  
      PRINTF("wurrdc: drop duplicate link layer packet %u\n",
             packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
    } else {
      mac_sequence_register_seqno();
    }
#endif /* RDC_WITH_DUPLICATE_DETECTION */
#endif /* WURRDC_802154_AUTOACK */

/* TODO We may want to acknowledge only authentic frames */ 
#if WURRDC_SEND_802154_ACK

    {
      frame802154_t info154;
      frame802154_parse(original_dataptr, original_datalen, &info154);
      if(info154.fcf.frame_type == FRAME802154_DATAFRAME &&
         info154.fcf.ack_required != 0 &&
         linkaddr_cmp((linkaddr_t *)&info154.dest_addr,
                      &linkaddr_node_addr)) {
        uint8_t ackdata[ACK_LEN] = {0, 0, 0};

        ackdata[0] = FRAME802154_ACKFRAME;
        ackdata[1] = 0;
        ackdata[2] = info154.seq;
        NETSTACK_RADIO.send(ackdata, ACK_LEN);

      }
    }
#endif /* WURRDC_SEND_ACK */

// WUR optimisation
    if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_node_addr)) {
      off();
    }

    if(!duplicate) {
      NETSTACK_MAC.input();   
    }
  }
	  off();

}
/*---------------------------------------------------------------------------*/

PROCESS(wur_process, "wur event handler process");

static void
init(void)
{
  printf("wurrdc: Initialized\n");
  wur_init();

  process_start(&wur_process, NULL);
  on();
}
/*---------------------------------------------------------------------------*/
static int
turn_on(void)
{
  return NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
static int
turn_off(int keep_radio_on)
{
  if(keep_radio_on) {
    return NETSTACK_RADIO.on();
  } else {
    return NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return 0;
}

/*---------------------------------------------------------------------------*/
const struct rdc_driver wurrdc_driver = {
  "wurrdc",
  init,
  send_packet,
  send_list,
  packet_input,
  turn_on,
  turn_off,
  channel_check_interval,
};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(wur_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();
  SENSORS_ACTIVATE(wur_sensor); //for enabling wake-up radio
  
  etimer_set(&timer, CLOCK_SECOND/64);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
  off();

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&
           data == &wur_sensor);

  if ((WUR_RX_LENGTH == LINKADDR_SIZE) && \
            (linkaddr_cmp((linkaddr_t *)&WUR_RX_BUFFER, &linkaddr_null)/*bcast*/ || \
            linkaddr_cmp((linkaddr_t *)&WUR_RX_BUFFER, &linkaddr_node_addr)/*ucast*/)){

			on(); //turn on the radio on the receiver side
			etimer_set(&timer, 3); //timeout for the reception of data packet
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
			off(); //turn off radio after reception
	 }
  }
  PROCESS_END();
}
