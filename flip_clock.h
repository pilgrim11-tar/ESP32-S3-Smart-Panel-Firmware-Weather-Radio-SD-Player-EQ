#ifndef FLIP_CLOCK_H
#define FLIP_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui.h"

void flip_clock_create(lv_obj_t *parent);
bool flip_clock_set_datetime(uint8_t hour24, uint8_t minute, uint8_t second, const char *week_text, const char *date_text, const char *holiday_text, const char *timezone_text);

#ifdef __cplusplus
}
#endif

#endif

