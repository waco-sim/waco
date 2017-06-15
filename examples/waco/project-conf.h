#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

#define CC2420_CONF_AUTOACK 1
#define WURRDC_CONF_802154_AUTOACK 1

/* Netstack layers */
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     wurrdc_driver  /* activating wake-up radio driver*/

#endif /* __PROJECT_CONF_H__ */


