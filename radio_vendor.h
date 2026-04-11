#pragma once

#include <Arduino.h>
#include <stdbool.h>
#include <stddef.h>

struct VendorRadioStation {
    const char *name;
    const char *url;
};

void radio_vendor_init();
void radio_vendor_loop();
void radio_vendor_stop();
bool radio_vendor_start_index(int idx);
bool radio_vendor_start_random();
bool radio_vendor_next();
bool radio_vendor_is_playing();
int radio_vendor_current_index();
uint8_t radio_vendor_get_volume_percent();
void radio_vendor_set_volume_percent(uint8_t percent);
void radio_vendor_set_output_muted(bool muted);
bool radio_vendor_output_muted();
const VendorRadioStation *radio_vendor_stations(size_t *count);
const char *radio_vendor_current_name();
const char *radio_vendor_current_title();
const char *radio_vendor_current_group();
bool radio_vendor_play_test();
bool radio_vendor_play_speech(const char *text);
bool radio_vendor_play_sd(const char *path);
bool radio_vendor_play_sd_with_info(const char *path, const char *title, const char *group);
void radio_vendor_set_media_info(const char *title, const char *group);
void radio_vendor_set_pinout_mode(uint8_t mode);
uint8_t radio_vendor_get_pinout_mode();
bool radio_vendor_backend_ready();
const char *radio_vendor_last_status();
uint16_t radio_vendor_get_vu_level();

