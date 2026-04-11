#pragma once

#include <Arduino.h>

void mic_remote_client_init();
void mic_remote_client_loop();
void mic_remote_client_force_refresh();
void mic_remote_client_suspend_polling(bool suspend);
bool mic_remote_client_polling_suspended();
bool mic_remote_client_listen_start(uint32_t wait_ms, uint32_t max_ms, uint32_t silence_ms);
bool mic_remote_client_listen_stop();
bool mic_remote_client_download_wav(const char *sd_path);
void mic_remote_client_set_visual_idle();
void mic_remote_client_set_visual_armed();
void mic_remote_client_set_visual_cooldown(uint32_t remaining_ms, uint32_t total_ms);
void mic_remote_client_set_visual_disabled();
void mic_remote_client_push_vu(const uint8_t *levels, size_t count, bool active);
bool mic_remote_client_set_command_language(const char *code);
bool mic_remote_client_bt_start();
bool mic_remote_client_bt_stop();
bool mic_remote_client_radio_start_index(int idx);
bool mic_remote_client_radio_next();
bool mic_remote_client_radio_stop();
bool mic_remote_client_radio_playing();
int mic_remote_client_radio_index();

bool mic_remote_client_online();
bool mic_remote_client_ready();
bool mic_remote_client_voice();
bool mic_remote_client_listen_active();
bool mic_remote_client_recording();
bool mic_remote_client_clip_ready();
uint16_t mic_remote_client_consume_command();
uint32_t mic_remote_client_rms();
uint32_t mic_remote_client_rms_smooth();
uint32_t mic_remote_client_noise_floor();
uint32_t mic_remote_client_gate_on();
uint32_t mic_remote_client_peak();
const char *mic_remote_client_status();
const char *mic_remote_client_listen_phase();
bool mic_remote_client_consume_changed();







