#pragma once

#include <Arduino.h>

#include "lvgl.h"

void home_bg_sd_assets_init();
bool home_bg_sd_assets_apply(lv_obj_t *image_obj, uint8_t month, uint8_t hour);
String home_bg_sd_assets_key_for(uint8_t month, uint8_t hour);
