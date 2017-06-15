#ifndef WUR_H
#define WUR_H

#include "lib/sensors.h"

extern const struct sensors_sensor wur_sensor;
void wur_init();
void wur_set_tx();
void wur_clear_tx();

#endif
