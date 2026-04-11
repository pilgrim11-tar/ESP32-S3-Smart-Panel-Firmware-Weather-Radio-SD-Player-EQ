#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "lvgl.h"

void weather_sd_assets_init();
void weather_sd_assets_poll_upload();
bool weather_sd_assets_session_active();
bool weather_sd_assets_has_animation(const char *state_name);
uint8_t weather_sd_assets_frame_count(const char *state_name);
bool weather_sd_assets_load_frame(const char *state_name, uint8_t frame_index, const lv_img_dsc_t **out_dsc);
