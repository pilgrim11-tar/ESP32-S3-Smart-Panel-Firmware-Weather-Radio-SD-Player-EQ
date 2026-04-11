#pragma once

#include <stdint.h>

struct WakeLogicInputs
{
    bool donor_wake_enabled = false;
    bool donor_gpio_high = false;
    uint32_t now_ms = 0;
    uint32_t donor_wake_hold_ms = 0;

    bool system_soft_off = false;
    bool media_output_active = false;

    bool donor_sound_wake_enabled = false;
    bool mic_online = false;
    bool mic_voice = false;
    bool display_sleeping = false;

    uint32_t noise_floor = 0;
    uint32_t gate_on = 0;
    uint32_t rms_raw = 0;
    uint32_t rms_smooth = 0;
    uint32_t peak = 0;
};

void wake_logic_reset();
bool wake_logic_update(const WakeLogicInputs &in);

