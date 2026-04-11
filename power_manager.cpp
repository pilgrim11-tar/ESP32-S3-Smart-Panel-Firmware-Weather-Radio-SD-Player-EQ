#include "power_manager.h"

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
    uint8_t soft_off_backlight)
{
    if (!last_user_activity_ms || !last_sound_activity_ms || !display_wake_hold_until_ms || !display_sleeping)
    {
        return -1;
    }

    if (*last_user_activity_ms == 0)
    {
        *last_user_activity_ms = now_ms;
        *last_sound_activity_ms = now_ms;
    }

    if (system_soft_off)
    {
        *display_sleeping = false;
        *display_wake_hold_until_ms = 0;
        return static_cast<int16_t>(soft_off_backlight);
    }

    if (alarm_active || mic_active)
    {
        *last_sound_activity_ms = now_ms;
        *display_wake_hold_until_ms = now_ms + wake_hold_ms;
        if (*display_sleeping)
        {
            *display_sleeping = false;
            return static_cast<int16_t>(user_backlight);
        }
        return -1;
    }

    if (now_ms < *display_wake_hold_until_ms)
    {
        return -1;
    }

    const uint32_t lastActivity = (*last_user_activity_ms > *last_sound_activity_ms) ? *last_user_activity_ms : *last_sound_activity_ms;
    if (!*display_sleeping && (now_ms - lastActivity) >= idle_ms)
    {
        *display_sleeping = true;
        return static_cast<int16_t>(sleep_backlight);
    }

    return -1;
}

int16_t power_manager_note_user_activity(
    uint32_t now_ms,
    bool system_soft_off,
    uint32_t *last_user_activity_ms,
    bool *display_sleeping,
    uint8_t user_backlight)
{
    if (!last_user_activity_ms || !display_sleeping)
    {
        return -1;
    }

    *last_user_activity_ms = now_ms;
    if (system_soft_off)
    {
        return -1;
    }
    if (*display_sleeping)
    {
        *display_sleeping = false;
        return static_cast<int16_t>(user_backlight);
    }
    return -1;
}

