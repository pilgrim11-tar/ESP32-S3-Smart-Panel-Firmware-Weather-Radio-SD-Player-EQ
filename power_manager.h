#pragma once

#include <stdint.h>

// Returns backlight level [0..255] if it should change, otherwise -1.
int16_t power_manager_refresh(
    uint32_t now_ms,
    uint32_t *last_user_activity_ms,
    uint32_t *last_sound_activity_ms,
    uint32_t *display_wake_hold_until_ms,
    bool *display_sleeping,
    bool system_soft_off,
    bool alarm_active,
    bool mic_active,
    uint32_t idle_ms,
    uint32_t wake_hold_ms,
    uint8_t user_backlight,
    uint8_t sleep_backlight,
    uint8_t soft_off_backlight);

// Returns backlight level [0..255] if it should change, otherwise -1.
int16_t power_manager_note_user_activity(
    uint32_t now_ms,
    bool system_soft_off,
    uint32_t *last_user_activity_ms,
    bool *display_sleeping,
    uint8_t user_backlight);

