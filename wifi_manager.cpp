#include "wifi_manager.h"

#include <WiFi.h>
#include <time.h>

void wifi_manager_connect_blocking(
    const String &ssid,
    const String &password,
    volatile int *connect_state,
    WifiManagerPostConnectHook post_connect_hook)
{
    if (connect_state)
    {
        *connect_state = 1;
    }

    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.persistent(false);

    if (ssid.length() == 0)
    {
        Serial.println("[wifi] empty ssid, skip connect");
        if (connect_state)
        {
            *connect_state = 3;
        }
        if (post_connect_hook)
        {
            post_connect_hook();
        }
        return;
    }

    bool connected = false;
    for (int attempt = 0; attempt < 1 && !connected; ++attempt)
    {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(false, false);
        delay(120);
        WiFi.begin(ssid.c_str(), password.c_str());

        const uint32_t wait_started = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(250);
            Serial.print(".");
            if ((millis() - wait_started) > 12000UL)
            {
                break;
            }
        }
        connected = (WiFi.status() == WL_CONNECTED);
    }

    if (connected)
    {
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        if (connect_state)
        {
            *connect_state = 2;
        }
    }
    else
    {
        Serial.printf("WiFi connect failed, status=%d\n", static_cast<int>(WiFi.status()));
        if (connect_state)
        {
            *connect_state = 3;
        }
    }

    if (post_connect_hook)
    {
        post_connect_hook();
    }
}

void wifi_manager_ntp_begin(const char *primary_server, const char *secondary_server, const char *timezone_spec)
{
    configTime(0, 0, primary_server, secondary_server);
    setenv("TZ", timezone_spec, 1);
    tzset();
}

bool wifi_manager_ntp_wait_initial_sync(uint8_t max_attempts, uint16_t delay_ms)
{
    time_t now_ts = time(nullptr);
    uint8_t attempts = 0;
    while (now_ts < 100000 && attempts < max_attempts)
    {
        delay(delay_ms);
        now_ts = time(nullptr);
        attempts++;
    }
    return now_ts >= 100000;
}

bool wifi_manager_ntp_read_time(WifiManagerTimeSnapshot *snapshot)
{
    if (!snapshot)
    {
        return false;
    }

    const time_t current_ts = time(nullptr);
    struct tm timeinfo;
    localtime_r(&current_ts, &timeinfo);

    snapshot->ntp_synced = current_ts >= 100000;
    snapshot->year = timeinfo.tm_year + 1900;
    snapshot->month = timeinfo.tm_mon + 1;
    snapshot->day = timeinfo.tm_mday;
    snapshot->hour = timeinfo.tm_hour;
    snapshot->minute = timeinfo.tm_min;
    snapshot->second = timeinfo.tm_sec;
    snapshot->weekday = timeinfo.tm_wday;
    snapshot->unix_time = static_cast<unsigned long>(current_ts);
    snapshot->is_dst = timeinfo.tm_isdst > 0;
    return true;
}

bool wifi_manager_load_preferences(Preferences &prefs, WifiManagerStoredSettings *settings)
{
    if (!settings)
    {
        return false;
    }

    if (!prefs.begin("wifi", true))
    {
        return false;
    }

    settings->ssid = prefs.getString("ssid", "");
    settings->password = prefs.getString("pswd", "");
    settings->city_query = prefs.getString("city", "");
    settings->weather_saved_lat = prefs.getFloat("wlat", 0.0f);
    settings->weather_saved_lon = prefs.getFloat("wlon", 0.0f);
    settings->voice_command_language = prefs.getUChar("vcmd_lang", 0);
    settings->alarm_enabled = prefs.getBool("alarm_on", false);
    settings->alarm_hour = prefs.getUChar("alarm_h", 7);
    settings->alarm_minute = prefs.getUChar("alarm_m", 0);
    settings->alarm_melody = prefs.getUChar("alarm_tone", 1);
    prefs.end();
    return true;
}

bool wifi_manager_save_preferences(Preferences &prefs, WifiManagerStoredSettings *settings)
{
    if (!settings)
    {
        return false;
    }

    if (!prefs.begin("wifi", false))
    {
        return false;
    }

    // Preserve known working credentials when caller saves unrelated settings.
    if (!settings->ssid.length() || !settings->password.length())
    {
        const String stored_ssid = prefs.getString("ssid", "");
        const String stored_password = prefs.getString("pswd", "");
        if (!settings->ssid.length() && stored_ssid.length())
        {
            settings->ssid = stored_ssid;
        }
        if (!settings->password.length() && stored_password.length())
        {
            settings->password = stored_password;
        }
    }

    prefs.putString("ssid", settings->ssid);
    prefs.putString("pswd", settings->password);
    prefs.putString("city", settings->city_query);
    prefs.putFloat("wlat", settings->weather_saved_lat);
    prefs.putFloat("wlon", settings->weather_saved_lon);
    prefs.putUChar("vcmd_lang", settings->voice_command_language);
    prefs.putBool("alarm_on", settings->alarm_enabled);
    prefs.putUChar("alarm_h", settings->alarm_hour);
    prefs.putUChar("alarm_m", settings->alarm_minute);
    prefs.putUChar("alarm_tone", settings->alarm_melody);
    prefs.end();
    return true;
}
