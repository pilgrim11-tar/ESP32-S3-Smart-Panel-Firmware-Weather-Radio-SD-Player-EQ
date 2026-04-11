#pragma once

#include <Arduino.h>
#include <lvgl.h>

void audio_visualizer_init();
void audio_visualizer_attach(lv_obj_t *parent);
void audio_visualizer_update_ui();

bool audio_visualizer_copy_levels(uint8_t *out_levels, size_t count);

