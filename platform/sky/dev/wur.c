#include "contiki.h"
#include "wur.h"
#include "dev/hwconf.h"
#include "isr_compat.h"
#include "dev/leds.h"

const struct sensors_sensor wur_sensor;

HWCONF_PIN(WUR_RX, 2, 0); //GPIO port to RX the wake-up signal
HWCONF_IRQ(WUR_RX, 2, 0);
HWCONF_PIN(WUR_TX, 2, 1); //GPIO port to TX the wake-up signal

ISR(PORT2, irq_p2)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);
  if(WUR_RX_CHECK_IRQ()) {
    leds_on(LEDS_RED);
      sensors_changed(&wur_sensor);
      leds_off(LEDS_RED);

      LPM4_EXIT;

  }
  P2IFG = 0x00;
  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}

static int value(int type)
{
  return WUR_RX_READ();
}
/*---------------------------------------------------------------------------*/
static int status(int type)
{
  switch (type) {
  case SENSORS_ACTIVE:
  case SENSORS_READY:
    return WUR_RX_IRQ_ENABLED();
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int configure(int type, int c)
{
  switch (type) {
  case SENSORS_ACTIVE:
    if (c) {
      if(!status(SENSORS_ACTIVE)) {
	       WUR_RX_IRQ_EDGE_SELECTU();

	       WUR_RX_SELECT();
	       WUR_RX_MAKE_INPUT();
       
	       WUR_RX_ENABLE_IRQ();
      }
    } else {
        WUR_RX_DISABLE_IRQ();
    }
    return 1;
  }
  return 0;
}

void wur_init() {
	WUR_TX_SELECT();
	WUR_TX_MAKE_OUTPUT();
	WUR_TX_CLEAR();
}

void wur_set_tx() {
	WUR_TX_SET();
}

void wur_clear_tx() {
	WUR_TX_CLEAR();
}
/*---------------------------------------------------------------------------*/
SENSORS_SENSOR(wur_sensor, "wur_rx_sensor",
	       value, configure, status);

SENSORS(&wur_sensor);