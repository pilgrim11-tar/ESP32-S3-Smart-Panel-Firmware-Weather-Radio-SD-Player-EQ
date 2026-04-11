#pragma once

#include <Arduino.h>
#include <Preferences.h>

using WifiManagerPostConnectHook = void (*)();

struct WifiManagerTimeSnapshot
{
    bool ntp_synced = false;
    int year = 1970;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int weekday = 4;
    unsigned long unix_time = 0;
    bool is_dst = false;
};

struct WifiManagerStoredSettings
{
    String ssid;
    String password;
    String city_query;
    float weather_saved_lat = 0.0f;
    float weather_saved_lon = 0.0f;
    uint8_t voice_command_language = 0;
    bool alarm_enabled = false;
    uint8_t alarm_hour = 7;
    uint8_t alarm_minute = 0;
    uint8_t alarm_melody = 1;
};

void wifi_manager_connect_blocking(
    const String &ssid,
    const String &password,
    volatile int *connect_state,
    WifiManagerPostConnectHook post_connect_hook);

void wifi_manager_ntp_begin(const char *primary_server, const char *secondary_server, const char *timezone_spec);
bool wifi_manager_ntp_wait_initial_sync(uint8_t max_attempts, uint16_t delay_ms);
bool wifi_manager_ntp_read_time(WifiManagerTimeSnapshot *snapshot);
bool wifi_manager_load_preferences(Preferences &prefs, WifiManagerStoredSettings *settings);
bool wifi_manager_save_preferences(Preferences &prefs, WifiManagerStoredSettings *settings);
