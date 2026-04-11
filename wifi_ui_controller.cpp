#include "wifi_ui_controller.h"

#include <WiFi.h>

const char *wifi_ui_status_scanning()
{
    return "Scan...";
}

const char *wifi_ui_status_after_scan(int network_count)
{
    return network_count > 0 ? "Please connect to WiFi" : "No WiFi found";
}

const char *wifi_ui_status_connecting()
{
    return "Connect...";
}

const char *wifi_ui_status_connect_success()
{
    return "Success";
}

const char *wifi_ui_status_connect_failed()
{
    return "Failed";
}

const char *wifi_ui_status_connect_task_failed()
{
    return "Connect failed";
}

String wifi_ui_networks_to_dropdown_options(int network_count)
{
    if (network_count <= 0)
    {
        return "No networks found";
    }

    String options;
    options.reserve(static_cast<size_t>(network_count) * 16U);
    for (int i = 0; i < network_count; ++i)
    {
        options += WiFi.SSID(i);
        if (i < (network_count - 1))
        {
            options += "\n";
        }
    }
    return options;
}

String wifi_ui_current_connection_text(wl_status_t wifi_status, const String &ssid)
{
    if (wifi_status != WL_CONNECTED)
    {
        return "WiFi: offline";
    }

    if (ssid.length() == 0)
    {
        return "WiFi: connected";
    }

    String name = ssid;
    if (name.length() > 18)
    {
        name = name.substring(0, 18) + "...";
    }
    return "WiFi: " + name;
}

void wifi_ui_textarea_focus_cb(lv_event_t *e)
{
    if (!e || lv_event_get_code(e) != LV_EVENT_FOCUSED)
    {
        return;
    }

    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *keyboard = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
    if (keyboard && target)
    {
        lv_keyboard_set_textarea(keyboard, target);
    }
}

void wifi_ui_apply_saved_city_text(lv_obj_t *city_textarea, const String &city_query)
{
    if (!city_textarea || city_query.length() == 0)
    {
        return;
    }
    lv_textarea_set_text(city_textarea, city_query.c_str());
}
