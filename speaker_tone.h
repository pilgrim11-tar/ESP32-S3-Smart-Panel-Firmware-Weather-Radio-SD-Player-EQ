#pragma once

#include <Arduino.h>

bool speaker_tone_start();
bool speaker_tone_is_busy();
bool speaker_tone_click();
bool speaker_tone_action();
bool speaker_tone_alarm(uint8_t intensity_level);
bool speaker_tone_alarm_variant(uint8_t melody_id, uint8_t intensity_level);
void speaker_tone_stop();
