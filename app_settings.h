#pragma once

#include <Arduino.h>

namespace app_settings {

// Display sleep / wake timings.
static constexpr uint32_t kDisplaySleepIdleMs = 180000;
static constexpr uint32_t kDisplaySleepWakeHoldMs = 4000;

// Display brightness policies.
static constexpr uint8_t kDisplaySleepBrightness = 24;
static constexpr uint8_t kSystemSoftOffBrightness = 34;

// Mic reactive behavior windows.
static constexpr uint32_t kMicReactiveMinVoiceMs = 260;
static constexpr uint32_t kMicReactiveCooldownMs = 12000;
static constexpr uint32_t kMicReactiveNightCooldownMs = 20000;

} // namespace app_settings

