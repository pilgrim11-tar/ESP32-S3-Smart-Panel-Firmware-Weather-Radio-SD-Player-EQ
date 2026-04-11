#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "lvgl.h"

void weather_visuals_init();
bool weather_visuals_is_animated(int weather_code, bool is_day);
void weather_visuals_apply(lv_obj_t *img, uint8_t *frame_index, bool reset_frame, int weather_code, bool is_day);
